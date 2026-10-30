use adblock::lists::{FilterSet, ParseOptions};
fn main() {
    let mut filters=FilterSet::new(true);
    for file in std::env::args().skip(1) { filters.add_filter_list(std::fs::read_to_string(file).expect("filter list"),ParseOptions::default()); }
    let (rules, converted)=filters.into_content_blocking().expect("content rule conversion");
    eprintln!("Converted {} source rules to {} WebKit rules",converted.len(),rules.len());
    println!("{}",serde_json::to_string(&rules).unwrap());
}
