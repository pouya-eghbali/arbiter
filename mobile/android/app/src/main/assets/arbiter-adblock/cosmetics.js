let timer;
function update() {
  clearTimeout(timer); timer=setTimeout(async () => {
    const elements=[...document.querySelectorAll('[class],[id]')].slice(0,2000);
    const classes=[...new Set(elements.flatMap(e=>[...e.classList]))].slice(0,500);
    const ids=elements.map(e=>e.id).filter(Boolean).slice(0,500);
    const styles=await browser.runtime.sendMessage({type:'cosmetics',classes,ids}).catch(()=>null);
    if (!styles) return;
    let style=document.getElementById('__arbiter_cosmetics');
    if (!style) {style=document.createElement('style');style.id='__arbiter_cosmetics';(document.head||document.documentElement).append(style);}
    if (style.textContent!==styles) style.textContent=styles;
  },200);
}
update(); new MutationObserver(update).observe(document.documentElement,{childList:true,subtree:true});
