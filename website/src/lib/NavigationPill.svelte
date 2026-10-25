<script lang="ts">
  import Icon from './Icon.svelte';
  let { collapsed = false, large = false }: { collapsed?: boolean; large?: boolean } = $props();
</script>

<div
  class="navigation-pill"
  class:collapsed
  class:large
  role="img"
  aria-label={collapsed
    ? 'Arbiter’s collapsed navigation pill'
    : 'Arbiter navigation: back, forward, reload, address, ad blocking, share and tabs'}
>
  <span class="pill-content"
    ><Icon name="arrow-left" /><Icon name="arrow-right" /><Icon name="rotate-cw" /><span
      class="pill-address"><Icon name="lock" size={13} /><span>asterjournal.com</span></span
    ><Icon name="shield-check" /><Icon name="share" /><Icon name="layers" /></span
  >
  <span class="pill-handle"></span>
</div>
