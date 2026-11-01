<script lang="ts">
  import MobilePreviewPhone from './MobilePreviewPhone.svelte';
</script>

<div class="mobile-demo">
  <div class="phones">
    <MobilePreviewPhone platform="iOS" /><MobilePreviewPhone
      platform="Android"
      initialView="tabs"
    />
  </div>
</div>
<p class="demo-hint">INTERACTIVE PREVIEW <span>Open a tab. Try the pill. Switch spaces. ↗</span></p>

<style>
  .mobile-demo {
    position: relative;
    container-type: inline-size;
    border-radius: 18px;
    overflow: clip;
    background: #ffe995;
    isolation: isolate;
  }
  .mobile-demo::before {
    content: '';
    position: absolute;
    width: 120%;
    height: 42%;
    left: -9%;
    bottom: -8%;
    border-radius: 50% 50% 0 0;
    background: #b5bba5;
    transform: rotate(-12deg);
    z-index: -1;
  }
  .mobile-demo::after {
    content: '';
    position: absolute;
    width: 55%;
    aspect-ratio: 1;
    border-radius: 50%;
    background: #f6d96d;
    left: -15%;
    top: 15%;
    z-index: -2;
  }
  .phones {
    display: flex;
    gap: 2%;
    align-items: center;
    justify-content: center;
    padding: 6% 3%;
  }
  .demo-hint {
    display: flex;
    flex-wrap: wrap;
    gap: 8px 16px;
    justify-content: space-between;
    margin-top: 17px;
    font-size: 8px;
    line-height: 1.6;
    letter-spacing: 1px;
    color: #66685d;
  }
  .demo-hint span {
    font-size: 10px;
    letter-spacing: 0;
  }
</style>
