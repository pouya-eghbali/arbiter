<script lang="ts">
  let { kind }: { kind: 'mobile' | 'sync' } = $props();
  // Frame the two illustrations in the approved generated concept. Keep the
  // original artwork intact; headings, links and feature copy are real HTML.
  const regions = {
    mobile: { x: 494, y: 94, width: 610, height: 606 },
    sync: { x: 480, y: 778, width: 628, height: 529 }
  };
  const labels = {
    mobile:
      'Illustrated preview of Arbiter on iOS and Android, with a floating navigation pill and tab overview.',
    sync: 'Illustrated preview of Arbiter’s desktop and mobile Sync settings, with pairing controls and connected devices.'
  };
  let region = $derived(regions[kind]);
</script>

<div class="artwork" style:aspect-ratio="{region.width} / {region.height}">
  <img
    src="/art/mobile-sync-preview.png"
    alt={labels[kind]}
    width="1150"
    height="1368"
    loading="lazy"
    decoding="async"
    style:width="{(1150 / region.width) * 100}%"
    style:left="{(-region.x / region.width) * 100}%"
    style:top="{(-region.y / region.height) * 100}%"
  />
</div>

<style>
  .artwork {
    position: relative;
    overflow: hidden;
    border-radius: 18px;
    width: 100%;
  }
  img {
    position: absolute;
    max-width: none;
    height: auto;
    pointer-events: none;
    user-select: none;
  }
</style>
