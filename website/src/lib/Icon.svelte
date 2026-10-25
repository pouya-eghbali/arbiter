<script lang="ts">
  let { name, size = 20 }: { name: string; size?: number } = $props();
  const icons = import.meta.glob<string>('./icons/*.svg', {
    eager: true,
    query: '?raw',
    import: 'default'
  });
</script>

<span class="icon" style:width="{size}px" style:height="{size}px" aria-hidden="true">
  {@html icons[`./icons/${name}.svg`] ?? ''}
</span>

<style>
  .icon {
    display: inline-flex;
    flex: 0 0 auto;
    vertical-align: middle;
  }
  .icon :global(svg) {
    width: 100%;
    height: 100%;
    stroke-width: 1.7;
  }
</style>
