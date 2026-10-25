<script lang="ts">
  import Journal from './Journal.svelte';
  import NavigationPill from './NavigationPill.svelte';
  import SplitDemo from './SplitDemo.svelte';
  import WorkspaceDemo from './WorkspaceDemo.svelte';
  import Icon from './Icon.svelte';
  const features = ['The pill', 'Split views', 'Workspaces', 'Make it yours'];
  const captions = [
    'Your page, from edge to edge. Navigation when you need it.',
    'Two, three, or more. Drag the divider to make room.',
    'Different contexts. A familiar place for each.',
    'Your colors. Your mood. Your kind of browser.'
  ];
  const colors = ['#7152aa', '#23756d', '#b24c33', '#2c62a1'];
  let selected = $state(0);
  let collapsed = $state(false);
  let accent = $state(colors[0]);
  let dark = $state(false);
  function navigate(event: KeyboardEvent, index: number) {
    if (['ArrowLeft', 'ArrowRight', 'Home', 'End'].includes(event.key)) {
      event.preventDefault();
      selected =
        event.key === 'Home'
          ? 0
          : event.key === 'End'
            ? 3
            : (index + (event.key === 'ArrowRight' ? 1 : -1) + 4) % 4;
      document.getElementById('feature-tab-' + selected)?.focus();
    }
  }
</script>

<div
  class="hero-demo"
  class:split-stage={selected === 1}
  class:workspace-stage={selected === 2}
  class:theme-stage={selected === 3}
  style:--demo-accent={accent}
>
  <div class="hero-browser" class:custom-dark={dark && selected === 3}>
    <div
      id="feature-panel"
      role="tabpanel"
      aria-labelledby="feature-tab-{selected}"
      tabindex="0"
      class="hero-browser-content"
    >
      {#if selected === 1}<SplitDemo />{:else}
        <Journal />
        {#if selected === 2}
          <div class="workspace-backdrop" aria-hidden="true"></div>
          <div class="workspace-overlay"><WorkspaceDemo compact /></div>
        {/if}
        {#if selected === 3}<div class="theme-controls">
            <span>MAKE YOURSELF AT HOME</span>
            <div class="theme-swatches">
              {#each colors as color}<button
                  style:background={color}
                  class:chosen={color === accent}
                  aria-label="Try {color === colors[0]
                    ? 'violet'
                    : color === colors[1]
                      ? 'forest'
                      : color === colors[2]
                        ? 'terracotta'
                        : 'blue'} accent"
                  aria-pressed={accent === color}
                  onclick={() => (accent = color)}
                  >{#if accent === color}<Icon name="check" size={16} />{/if}</button
                >{/each}<span class="theme-divider"></span><button
                class="theme-mode"
                aria-label="Dark appearance"
                aria-pressed={dark}
                onclick={() => (dark = !dark)}><span>{dark ? 'Dark' : 'Light'}</span></button
              >
            </div>
          </div>{/if}
        <div class="hero-pill"><NavigationPill {collapsed} /></div>
      {/if}
    </div>
  </div>
  <div class="preview-bar">
    <span class="preview-label">TAKE A LOOK AROUND</span>
    <div role="tablist" aria-label="Explore Arbiter features">
      {#each features as feature, i}<button
          id="feature-tab-{i}"
          role="tab"
          aria-selected={selected === i}
          aria-controls="feature-panel"
          tabindex={selected === i ? 0 : -1}
          onclick={() => (selected = i)}
          onkeydown={(e) => navigate(e, i)}>{feature}</button
        >{/each}
    </div>
    <span class="demo-label">INTERACTIVE PREVIEW <span>↗</span></span>
  </div>
  <div class="preview-caption" aria-live="polite">
    <span>{captions[selected]}</span>{#if selected === 0}<button
        onclick={() => (collapsed = !collapsed)}
        >{collapsed ? 'Reveal the pill' : 'Tuck it away'}
        <Icon name={collapsed ? 'plus' : 'arrow-down'} size={14} /></button
      >{/if}
  </div>
</div>
