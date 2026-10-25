<script lang="ts">
  import Icon from './Icon.svelte';
  let { interactive = true }: { interactive?: boolean } = $props();
  let ratio = $state(47);
  let container: HTMLDivElement;
  let dragging = $state(false);
  let tasks = $state([true, true, false, false]);
  const taskNames = [
    'Refine type scale',
    'Update illustrations',
    'Draft introduction',
    'Consider edge cases'
  ];

  function move(event: PointerEvent) {
    if (!dragging || !container) return;
    const bounds = container.getBoundingClientRect();
    ratio = Math.max(30, Math.min(65, ((event.clientX - bounds.left) / bounds.width) * 100));
  }
  function start(event: PointerEvent) {
    dragging = true;
    (event.currentTarget as HTMLElement).setPointerCapture(event.pointerId);
    move(event);
  }
  function resizeKey(event: KeyboardEvent) {
    if (['ArrowLeft', 'ArrowRight', 'Home', 'End'].includes(event.key)) {
      event.preventDefault();
      ratio =
        event.key === 'Home'
          ? 30
          : event.key === 'End'
            ? 65
            : Math.max(30, Math.min(65, ratio + (event.key === 'ArrowLeft' ? -3 : 3)));
    }
  }
</script>

<div class="split-demo" class:dragging bind:this={container} style:--split="{ratio}%">
  <div class="type-page">
    <div class="wiki-brand">WIKIPEDIA <Icon name="search" size={14} /></div>
    <h3>Typography</h3>
    <div class="article-nav"><span>Article</span><span>Talk</span><span>Read</span></div>
    <p>
      <strong>Typography</strong> is the art and technique of arranging type to make written language
      legible, readable, and appealing when displayed.
    </p>
    <p>
      The arrangement of type involves selecting typefaces, point sizes, line lengths, line spacing,
      and letter spacing.
    </p>
    <div class="wiki-contents">
      <span>Contents</span
      >{#each ['History', 'Typefaces', 'Applications', 'Composition', 'See also'] as topic}<span
          >{topic}</span
        >{/each}
    </div>
  </div>
  {#if interactive}
    <button
      class="split-handle"
      role="slider"
      aria-label="Resize the split view demo"
      aria-valuemin={30}
      aria-valuemax={65}
      aria-valuenow={Math.round(ratio)}
      aria-valuetext={Math.round(ratio) + '% for the left pane'}
      aria-orientation="horizontal"
      onpointerdown={start}
      onpointermove={move}
      onpointerup={() => (dragging = false)}
      onpointercancel={() => (dragging = false)}
      onlostpointercapture={() => (dragging = false)}
      onkeydown={resizeKey}><span></span></button
    >
  {:else}<div class="split-handle decorative"><span></span></div>{/if}
  <div class="split-right">
    <div class="code-page">
      <span class="code-kicker">THE CSS HANDBOOK</span>
      <h3>Layout</h3>
      <p>The CSS grid module provides a foundation for arranging elements on a page.</p>
      <div class="code-tabs"><span>Overview</span><span>Examples</span><span>Reference</span></div>
      <pre><span class="code-yellow">.container</span> {'{'}{'\n'}  <span class="code-pink"
          >display</span
        >: grid;{'\n'}  <span class="code-pink">grid-template-columns</span
        >:{'\n'}    1fr 1fr;{'\n'}  <span class="code-pink">gap</span>: 2rem;{'\n'}{'}'}</pre>
    </div>
    <div class="notes-page">
      <div class="notes-title">
        <h3>Project notes</h3>
        <span>✳</span>
      </div>
      {#each taskNames as task, i}<label
          ><input type="checkbox" bind:checked={tasks[i]} disabled={!interactive} /><span
            >{task}</span
          ></label
        >{/each}<span class="notes-bottom">A LITTLE SPACE TO THINK.</span>
    </div>
  </div>
</div>
