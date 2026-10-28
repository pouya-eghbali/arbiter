/** Progressive enhancement: content remains visible without JavaScript. */
export function reveal(node: HTMLElement, delay = 0) {
  const motion = window.matchMedia('(prefers-reduced-motion: reduce)');
  if (motion.matches || !('IntersectionObserver' in window)) return;

  let animation: Animation | undefined;
  const show = () => {
    observer.disconnect();
    node.style.removeProperty('opacity');
    animation?.finish();
  };
  const observer = new IntersectionObserver(
    ([entry]) => {
      if (!entry.isIntersecting) return;
      observer.disconnect();
      node.style.removeProperty('opacity');
      animation = node.animate(
        [
          { opacity: 0, transform: 'translateY(20px)' },
          { opacity: 1, transform: 'translateY(0)' }
        ],
        { duration: 650, delay, easing: 'cubic-bezier(0.22, 1, 0.36, 1)', fill: 'backwards' }
      );
    },
    { threshold: 0.08, rootMargin: '0px 0px -24px 0px' }
  );

  node.style.opacity = '0';
  observer.observe(node);
  node.addEventListener('focusin', show);
  motion.addEventListener('change', show);
  return {
    destroy() {
      observer.disconnect();
      animation?.cancel();
      node.style.removeProperty('opacity');
      node.removeEventListener('focusin', show);
      motion.removeEventListener('change', show);
    }
  };
}

/** Keep native details semantics and animate both opening and closing. */
export function accordion(node: HTMLDetailsElement) {
  const summary = node.querySelector('summary');
  if (!summary) return;
  const motion = window.matchMedia('(prefers-reduced-motion: reduce)');
  let animation: Animation | undefined;
  let expanded = node.open;
  const settle = () => {
    animation?.cancel();
    animation = undefined;
    node.open = expanded;
    node.style.removeProperty('overflow');
  };
  const toggle = (event: MouseEvent) => {
    event.preventDefault();
    expanded = !expanded;
    if (motion.matches) {
      settle();
      return;
    }

    const start = node.getBoundingClientRect().height;
    animation?.cancel();
    node.open = true;
    const style = getComputedStyle(node);
    const border = parseFloat(style.borderTopWidth) + parseFloat(style.borderBottomWidth);
    const end = expanded
      ? node.getBoundingClientRect().height
      : summary.getBoundingClientRect().height + border;
    node.style.overflow = 'hidden';
    animation = node.animate([{ height: `${start}px` }, { height: `${end}px` }], {
      duration: 260,
      easing: 'cubic-bezier(0.22, 1, 0.36, 1)'
    });
    animation.onfinish = settle;
  };
  summary.addEventListener('click', toggle);
  motion.addEventListener('change', settle);
  return {
    destroy() {
      settle();
      summary.removeEventListener('click', toggle);
      motion.removeEventListener('change', settle);
    }
  };
}
