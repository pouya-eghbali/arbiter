<script lang="ts">
  import Icon from './Icon.svelte';
  let { compact = false }: { compact?: boolean } = $props();
  let selected = $state('Personal');
  type Tab = { title: string; host: string; image?: string; letter?: string };
  const spaces: { name: string; color: string; tabs: Tab[] }[] = [
    {
      name: 'Personal',
      color: '#f9d84c',
      tabs: [
        { title: 'Aster Journal', host: 'asterjournal.com', image: '/art/geometry.svg' },
        { title: 'House Plants', host: 'houseplants.example', image: '/art/botanical.svg' },
        { title: 'Objects of interest', host: 'objects.example', image: '/art/chair.svg' }
      ]
    },
    {
      name: 'Studio',
      color: '#b1a0ee',
      tabs: [
        { title: 'Typography', host: 'en.wikipedia.org', letter: 'W' },
        { title: 'CSS: Grid layout', host: 'developer.mozilla.org', letter: 'M' },
        { title: 'Project notes', host: 'notes.example', letter: 'N' }
      ]
    }
  ];
</script>

<div class="workspace-demo" class:compact>
  <div class="workspace-heading"><span>YOUR WORKSPACES</span><Icon name="layers" size={17} /></div>
  <div class="workspace-columns">
    {#each spaces as space}
      <div class="workspace-column" class:active={selected === space.name}>
        <button
          class="workspace-select"
          aria-pressed={selected === space.name}
          onclick={() => (selected = space.name)}
          ><span class="workspace-dot" style:background={space.color}></span>{space.name}<span
            class="workspace-selected"
            >{#if selected === space.name}<Icon name="check" size={16} />{:else}<Icon
                name="chevron-right"
                size={16}
              />{/if}</span
          ></button
        >
        <span class="workspace-meta"
          >3 tabs · {space.name === 'Personal' ? 'Shared' : 'Separate'} session</span
        >
        {#each space.tabs as tab, index}<div
            class="workspace-tab"
            class:current={selected === space.name && index === 0}
          >
            {#if tab.image}<img src={tab.image} alt="" width="40" height="40" />{:else}<span
                class="site-letter">{tab.letter}</span
              >{/if}<span><strong>{tab.title}</strong><small>{tab.host}</small></span>
          </div>{/each}
      </div>
    {/each}
  </div>
  <div class="workspace-caption">
    <span class="workspace-dot" style:background={spaces.find((s) => s.name === selected)?.color}
    ></span>{selected} is in focus <span>Try switching spaces ↗</span>
  </div>
</div>
