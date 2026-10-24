use adblock::{Engine, lists::{FilterSet, ParseOptions}, request::Request};
use std::ffi::{CStr, CString, c_char};
use std::panic::{catch_unwind, AssertUnwindSafe};
unsafe fn text<'a>(value: *const c_char) -> &'a str {
    if value.is_null() { "" } else { unsafe { CStr::from_ptr(value) }.to_str().unwrap_or("") }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn arbiter_adblock_new(rules: *const c_char) -> *mut Engine {
    catch_unwind(|| {
        let mut filters = FilterSet::new(false);
        filters.add_filter_list(unsafe { text(rules).to_owned() }, ParseOptions::default());
        Box::into_raw(Box::new(Engine::new_with_filter_set(filters)))
    }).unwrap_or(std::ptr::null_mut())
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn arbiter_adblock_free(engine: *mut Engine) {
    if !engine.is_null() { let _ = catch_unwind(AssertUnwindSafe(|| drop(unsafe { Box::from_raw(engine) }))); }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn arbiter_adblock_check(engine: *const Engine, url: *const c_char, source: *const c_char, kind: *const c_char, method: *const c_char) -> bool {
    if engine.is_null() { return false; }
    catch_unwind(AssertUnwindSafe(|| {
        let Ok(request) = Request::new(unsafe { text(url) }, unsafe { text(source) }, unsafe { text(kind) }, unsafe { text(method) }) else { return false; };
        unsafe { &*engine }.check_network_request(&request).should_block()
    })).unwrap_or(false)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn arbiter_adblock_cosmetic(engine: *const Engine, url: *const c_char, classes: *const c_char, ids: *const c_char) -> *mut c_char {
    if engine.is_null() { return std::ptr::null_mut(); }
    catch_unwind(AssertUnwindSafe(|| {
        let engine = unsafe { &*engine };
        let resources = engine.url_cosmetic_resources(unsafe { text(url) });
        let classes: Vec<String> = serde_json::from_str(unsafe { text(classes) }).unwrap_or_default();
        let ids: Vec<String> = serde_json::from_str(unsafe { text(ids) }).unwrap_or_default();
        let mut selectors: Vec<String> = resources.hide_selectors.into_iter().collect();
        if !resources.generichide { selectors.extend(engine.hidden_class_id_selectors(&classes, &ids, &resources.exceptions)); }
        CString::new(serde_json::to_string(&selectors).unwrap_or_default()).map(CString::into_raw).unwrap_or(std::ptr::null_mut())
    })).unwrap_or(std::ptr::null_mut())
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn arbiter_adblock_string_free(value: *mut c_char) {
    if !value.is_null() { drop(unsafe { CString::from_raw(value) }); }
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn syntax_and_exceptions() {
        let rules=CString::new("||ads.example^$third-party\n@@||ads.example/allowed.js\n##.advert\nnews.example##.sponsored\nnews.example#@#.advert").unwrap();
        unsafe {
            let engine=arbiter_adblock_new(rules.as_ptr()); assert!(!engine.is_null());
            let source=CString::new("https://news.example/").unwrap(); let kind=CString::new("script").unwrap(); let method=CString::new("GET").unwrap();
            let blocked=CString::new("https://ads.example/banner.js").unwrap(); let allowed=CString::new("https://ads.example/allowed.js").unwrap();
            assert!(arbiter_adblock_check(engine,blocked.as_ptr(),source.as_ptr(),kind.as_ptr(),method.as_ptr()));
            assert!(!arbiter_adblock_check(engine,allowed.as_ptr(),source.as_ptr(),kind.as_ptr(),method.as_ptr()));
            let classes=CString::new(r#"["advert"]"#).unwrap(); let ids=CString::new("[]").unwrap();
            let css=arbiter_adblock_cosmetic(engine,source.as_ptr(),classes.as_ptr(),ids.as_ptr());
            let selectors: Vec<String>=serde_json::from_str(text(css)).unwrap();
            assert!(selectors.contains(&".sponsored".to_owned())); assert!(!selectors.contains(&".advert".to_owned()));
            arbiter_adblock_string_free(css); arbiter_adblock_free(engine);
        }
    }
}
