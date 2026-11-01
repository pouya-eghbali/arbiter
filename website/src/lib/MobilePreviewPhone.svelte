<script lang="ts">
  import Icon from './Icon.svelte';
  import { untrack } from 'svelte';
  let {
    platform,
    initialView = 'page'
  }: { platform: 'iOS' | 'Android'; initialView?: 'page' | 'tabs' } = $props();
  type View = 'page' | 'tabs' | 'spaces' | 'address';
  const pages = [
    {
      id: 'journal',
      title: 'Aster Journal',
      host: 'asterjournal.com',
      letter: '✳',
      art: '/art/geometry.svg'
    },
    { id: 'wiki', title: 'Typography', host: 'en.wikipedia.org', letter: 'W', art: '' },
    { id: 'mdn', title: 'CSS: Grid layout', host: 'developer.mozilla.org', letter: 'M', art: '' },
    {
      id: 'notes',
      title: 'Project notes',
      host: 'notes.example',
      letter: 'N',
      art: '/art/botanical.svg'
    }
  ];
  let view = $state<View>(untrack(() => initialView));
  let current = $state('journal');
  let workspace = $state('Personal');
  let collections = $state<Record<string, string[]>>({
    Personal: pages.map((p) => p.id),
    Studio: ['mdn', 'notes']
  });
  let open = $derived(collections[workspace]);
  let query = $state('');
  let page = $derived(pages.find((p) => p.id === current) ?? pages[0]);
  let suggestions = $derived(
    pages.filter((p) => `${p.title} ${p.host}`.toLowerCase().includes(query.toLowerCase()))
  );
  function visit(id: string) {
    current = id;
    if (!open.includes(id)) collections[workspace] = [...open, id];
    view = 'page';
    query = '';
  }
</script>

<section
  class="phone"
  class:android={platform === 'Android'}
  class:light={view === 'page'}
  aria-label="{platform} browser preview"
>
  <div class="status">
    <span>9:41</span><span class="camera"></span><span class="reception" aria-hidden="true"
      ><i></i><i></i><i></i><b></b></span
    >
  </div>
  <div class="viewport" class:dark={view !== 'page'}>
    {#if view === 'page'}
      <!-- svelte-ignore a11y_no_noninteractive_tabindex (Scrollable content needs a keyboard focus target.) -->
      <div
        class="page-content"
        role="region"
        tabindex="0"
        aria-label="Scroll the {platform} sample page"
      >
        {#if current === 'journal'}
          <div class="journal-top">
            <span>Aster<span class="asterisk">✳</span></span><button
              aria-label="Read Aster notes in {platform} preview"
              onclick={() => visit('notes')}><Icon name="arrow-right" size={14} /></button
            >
          </div>
          <p class="kicker">A JOURNAL OF EVERYDAY DISCOVERIES</p>
          <img
            class="cover"
            src="/art/geometry.svg"
            width="600"
            height="850"
            alt="Orange architectural art print"
          />
          <h3>A journal of ideas, objects and a more considered life.</h3>
          <p>Thoughts, observations and tools for a calmer, more intentional everyday.</p>
          <button class="story-link" onclick={() => visit('notes')}
            >Read the journal <Icon name="arrow-right" size={12} /></button
          >
        {:else if current === 'wiki'}
          <p class="wordmark">WIKIPEDIA</p>
          <h3>Typography</h3>
          <div class="article-nav">Article · Read</div>
          <p>
            <strong>Typography</strong> is the art and technique of arranging type to make written language
            legible, readable, and appealing when displayed.
          </p>
          <h4>A little history</h4>
          <p>
            Type brings structure and personality to the things we read. Good spacing gives every
            word room to breathe.
          </p>
        {:else if current === 'mdn'}
          <p class="wordmark">MDN WEB DOCS</p>
          <h3>CSS: Grid layout</h3>
          <p>A two-dimensional layout system for the web.</p>
          <pre>.page &#123;
  display: grid;
  gap: 1rem;
&#125;</pre>
          <p>Build flexible layouts with rows and columns, and let your content find its place.</p>
        {:else}
          <p class="wordmark">PROJECT NOTES</p>
          <h3>A little space to think.</h3>
          <img
            class="notes-art"
            src="/art/botanical.svg"
            width="500"
            height="400"
            alt="An olive-green houseplant"
          />
          <p>Collect good ideas.<br />Make room for curiosity.<br />Keep the web human.</p>
          <button class="story-link" onclick={() => visit('journal')}
            >Back to Aster <Icon name="arrow-right" size={12} /></button
          >
        {/if}
      </div>
    {:else if view === 'tabs'}
      <div class="panel-content">
        <div class="app-brand"><img src="/arbiter.png" width="20" height="20" alt="" />Arbiter</div>
        <div class="tab-heading">
          <strong>Tabs</strong><button onclick={() => (view = 'spaces')}>Workspaces</button><button
            class="new-tab"
            aria-label="Add a tab in the {platform} preview"
            onclick={() => {
              view = 'address';
            }}><Icon name="plus" size={15} /></button
          >
        </div>
        <div class="tab-grid">
          {#each pages.filter((p) => open.includes(p.id)) as tab (tab.id)}
            <div class="tab-card">
              <button
                class="open-tab"
                aria-label="Open {tab.title} in {platform} preview"
                onclick={() => visit(tab.id)}
              >
                <span class="thumbnail" class:paper={tab.id === 'wiki'}
                  >{#if tab.art}<img src={tab.art} width="120" height="150" alt="" />{:else}<span
                      >{tab.letter}</span
                    >{/if}</span
                >
                <strong>{tab.title}</strong><small>{tab.host}</small>
              </button>
              <button
                class="close-tab"
                aria-label="Close {tab.title} in {platform} preview"
                onclick={() => {
                  const remaining = open.filter((id) => id !== tab.id);
                  collections[workspace] = remaining;
                  if (current === tab.id) current = remaining[0] ?? 'journal';
                }}><Icon name="x" size={11} /></button
              >
            </div>
          {/each}
        </div>
        {#if open.length === 0}<p class="empty">A little room for something new.</p>
          <button class="restore" onclick={() => (collections[workspace] = pages.map((p) => p.id))}
            >Restore sample tabs</button
          >{/if}
      </div>
    {:else if view === 'spaces'}
      <div class="panel-content workspace-panel">
        <div class="panel-heading">
          <h3>Workspaces</h3>
          <button aria-label="Close {platform} workspace picker" onclick={() => (view = 'tabs')}
            ><Icon name="x" size={15} /></button
          >
        </div>
        {#each ['Personal', 'Studio'] as space, i}<button
            class="space"
            aria-pressed={workspace === space}
            onclick={() => {
              workspace = space;
              current = collections[space][0] ?? 'journal';
              view = 'tabs';
            }}
            ><span class="dot" class:violet={i === 1}></span><span
              ><strong>{space}</strong><small
                >{i === 0 ? 'Shared session' : 'Separate session'}</small
              ></span
            >{#if workspace === space}<Icon name="check" size={14} />{/if}</button
          >{/each}
      </div>
    {:else}
      <div class="panel-content address-panel">
        <div class="panel-heading">
          <h3>Search</h3>
          <button aria-label="Close {platform} address preview" onclick={() => (view = 'page')}
            ><Icon name="x" size={15} /></button
          >
        </div>
        <form
          onsubmit={(e) => {
            e.preventDefault();
            if (suggestions[0]) visit(suggestions[0].id);
          }}
        >
          <input
            aria-label="Search sample pages in {platform} preview"
            bind:value={query}
            placeholder="Search sample pages"
            autocomplete="off"
            spellcheck="false"
          />
        </form>
        {#each suggestions as suggestion}<button
            class="suggestion"
            onclick={() => visit(suggestion.id)}
            ><span>{suggestion.letter}</span><span
              ><strong>{suggestion.title}</strong><small>{suggestion.host}</small></span
            ><Icon name="arrow-right" size={12} /></button
          >{/each}
        {#if !suggestions.length}<p class="empty">Try Wikipedia, MDN or Aster.</p>{/if}
      </div>
    {/if}
    <div class="pill" aria-label="{platform} navigation pill">
      {#if view === 'page'}<button
          class="pill-address"
          aria-label="Open the {platform} address preview"
          onclick={() => {
            query = '';
            view = 'address';
          }}><Icon name="lock" size={11} /><span>{page.host}</span></button
        >
      {:else}<button
          class="pill-address"
          aria-label="Search sample pages on {platform}"
          onclick={() => {
            query = '';
            view = 'address';
          }}><Icon name="search" size={12} /><span>Search or enter address</span></button
        >{/if}
      <button
        class="tab-count"
        aria-label="Show tabs in the {platform} preview"
        aria-pressed={view === 'tabs'}
        onclick={() => (view = view === 'tabs' && open.length ? 'page' : 'tabs')}
        >{open.length}</button
      >
      <button
        aria-label="Choose a workspace in the {platform} preview"
        aria-pressed={view === 'spaces'}
        onclick={() => (view = view === 'spaces' ? 'page' : 'spaces')}
        ><Icon name="ellipsis" size={15} /></button
      >
    </div>
  </div>
  <div class="home"><span></span></div>
</section>

<style>
  .phone {
    width: 43%;
    aspect-ratio: 390 / 844;
    border: 0.8cqw solid #17191b;
    border-radius: 6.2cqw;
    background: #f6f3e9;
    box-shadow:
      0 1.6cqw 2.6cqw #262a2b35,
      inset 0 0 0 1px #ffffff8c;
    position: relative;
    display: flex;
    flex-direction: column;
    overflow: hidden;
    color: #292b26;
  }
  .phone.android {
    background: #17202b;
    color: #f2f3f7;
    transform: rotate(1.6deg);
  }
  .phone.light {
    background: #f6f3e9;
    color: #292b26;
  }
  .viewport:not(.dark) {
    background: #f6f3e9;
    color: #292b26;
  }

  button {
    background: none;
    padding: 0;
    color: inherit;
    display: inline-flex;
    align-items: center;
    justify-content: center;
  }
  button:focus-visible,
  input:focus-visible {
    outline-offset: 2px;
    outline-width: 2px;
  }
  .status {
    position: relative;
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 1.8cqw 2.4cqw 1cqw;
    min-height: 5.9cqw;
    font-size: 1.3cqw;
    font-weight: 650;
  }
  .camera {
    position: absolute;
    width: 11cqw;
    height: 2.6cqw;
    border-radius: 20px;
    background: #080909;
    top: 1cqw;
    left: 50%;
    transform: translateX(-50%);
  }
  .android .camera {
    width: 1.6cqw;
    height: 1.6cqw;
    top: 1.2cqw;
  }
  .reception {
    display: flex;
    align-items: flex-end;
    gap: 0.3cqw;
  }
  .reception i {
    width: 0.45cqw;
    background: currentColor;
    height: 0.65cqw;
  }
  .reception i:nth-child(2) {
    height: 1cqw;
  }
  .reception i:nth-child(3) {
    height: 1.4cqw;
  }
  .reception b {
    width: 2.2cqw;
    height: 1.2cqw;
    margin-left: 0.6cqw;
    border: 1px solid;
    border-radius: 2px;
    background: currentColor;
  }
  .viewport {
    position: relative;
    flex: 1;
    min-height: 0;
    overflow: hidden;
  }
  .viewport.dark {
    color: #ecedf4;
    background: #17202b;
  }
  .page-content,
  .panel-content {
    height: 100%;
    overflow: auto;
    overscroll-behavior: contain;
    scrollbar-width: thin;
    padding: 2cqw 2.5cqw 10cqw;
  }
  .journal-top {
    display: flex;
    align-items: center;
    justify-content: space-between;
    font:
      4cqw Georgia,
      serif;
    margin-bottom: 3cqw;
  }
  .journal-top button {
    min-width: 3.5cqw;
    min-height: 3.5cqw;
  }
  .asterisk {
    font-size: 2.3cqw;
    margin-left: 0.6cqw;
  }
  .kicker {
    font-size: 0.85cqw !important;
    letter-spacing: 0.24cqw;
    margin-bottom: 2cqw !important;
  }
  .cover {
    width: 100%;
    height: 36cqw;
    object-fit: cover;
    margin-bottom: 2.2cqw;
  }
  .page-content h3 {
    font:
      3.4cqw/1.12 Georgia,
      serif;
    margin: 2cqw 0;
    letter-spacing: -0.1cqw;
  }
  .page-content p {
    font:
      1.8cqw/1.5 Georgia,
      serif;
    margin: 1.5cqw 0;
  }
  .page-content h4 {
    font-size: 2.1cqw;
    margin-top: 3cqw;
  }
  .wordmark {
    font:
      1.5cqw 'DM Sans Variable',
      sans-serif !important;
    letter-spacing: 0.2cqw;
    padding-block: 2cqw;
  }
  .article-nav {
    border-block: 1px solid #d2d2c8;
    padding-block: 1cqw;
    font-size: 1.4cqw;
  }
  pre {
    font-size: 1.6cqw;
    background: #e6e1d5;
    padding: 1.6cqw;
    white-space: pre-wrap;
    line-height: 1.6;
  }
  .notes-art {
    width: 100%;
    height: 23cqw;
    object-fit: cover;
  }
  .story-link {
    font-size: 1.7cqw;
    gap: 1cqw;
    border-bottom: 1px solid;
    padding-block: 1cqw;
  }
  .app-brand {
    display: flex;
    align-items: center;
    justify-content: center;
    gap: 0.8cqw;
    margin: 1.5cqw 0 4cqw;
    font-size: 2.3cqw;
    font-weight: 700;
  }
  .app-brand img {
    width: 3.6cqw;
    height: 3.6cqw;
  }
  .tab-heading {
    display: flex;
    gap: 2cqw;
    align-items: center;
    font-size: 1.5cqw;
    margin-bottom: 2.6cqw;
  }
  .tab-heading strong {
    border-bottom: 2px solid #b6a1f6;
    padding-block: 1cqw;
  }
  .new-tab {
    margin-left: auto;
    min-width: 3.5cqw;
    min-height: 3.5cqw;
  }
  .tab-grid {
    display: grid;
    grid-template-columns: repeat(2, minmax(0, 1fr));
    gap: 1.5cqw;
  }
  .tab-card {
    position: relative;
    min-width: 0;
    border-radius: 1.5cqw;
    background: #ffffff0b;
    border: 1px solid #ffffff0b;
    overflow: hidden;
  }
  .tab-card:hover {
    background: #ffffff15;
  }
  .open-tab {
    width: 100%;
    flex-direction: column;
    align-items: stretch;
    text-align: left;
    padding-bottom: 1.5cqw;
  }
  .open-tab strong {
    font-size: 1.45cqw;
    font-weight: 500;
    padding: 1.2cqw 1cqw 0.5cqw;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
  }
  .open-tab small {
    font-size: 1cqw;
    padding-inline: 1cqw;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
    color: #b5bec9;
  }
  .thumbnail {
    display: grid;
    place-items: center;
    height: 17cqw;
    background: #242d3b;
    overflow: hidden;
  }
  .thumbnail img {
    width: 100%;
    height: 100%;
    object-fit: cover;
  }
  .thumbnail > span {
    font-size: 6cqw;
  }
  .thumbnail.paper {
    background: #f8f6ee;
    color: #222;
    font-family: Georgia, serif;
  }
  .close-tab {
    position: absolute;
    right: 0.5cqw;
    top: 0.5cqw;
    border-radius: 50%;
    width: 3.2cqw;
    height: 3.2cqw;
    background: #121c25c9;
    color: white;
  }
  .panel-heading {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin: 2cqw 0 3cqw;
  }
  .panel-heading h3 {
    font-size: 2.7cqw;
    letter-spacing: -0.1cqw;
  }
  .panel-heading button {
    min-width: 3.5cqw;
    min-height: 3.5cqw;
  }
  .space {
    width: 100%;
    padding: 2cqw;
    gap: 1.3cqw;
    margin-bottom: 1.6cqw;
    background: #ffffff0a;
    border-radius: 2cqw;
    text-align: left;
  }
  .space[aria-pressed='true'] {
    background: #b7a2f62b;
  }
  .space > span:nth-child(2) {
    flex: 1;
  }
  .space strong,
  .space small,
  .suggestion strong,
  .suggestion small {
    display: block;
  }
  .space strong {
    font-size: 2cqw;
  }
  .space small {
    color: #b4bdca;
    font-size: 1.3cqw;
    margin-top: 0.5cqw;
  }
  .dot {
    width: 1.4cqw;
    height: 1.4cqw;
    border-radius: 50%;
    background: #ffe073;
  }
  .dot.violet {
    background: #b4a0f2;
  }
  input {
    width: 100%;
    background: #ffffff0c;
    border: 1px solid #ffffff29;
    border-radius: 1.7cqw;
    padding: 1.8cqw;
    color: inherit;
    font-size: 1.7cqw;
    margin-bottom: 2cqw;
    min-width: 0;
  }
  .suggestion {
    display: flex;
    width: 100%;
    gap: 1.3cqw;
    padding: 1.8cqw 0;
    text-align: left;
    border-bottom: 1px solid #ffffff0b;
  }
  .suggestion > span:first-child {
    font-size: 2.5cqw;
    width: 3cqw;
    text-align: center;
  }
  .suggestion > span:nth-child(2) {
    flex: 1;
    min-width: 0;
  }
  .suggestion strong {
    font-size: 1.7cqw;
  }
  .suggestion small {
    font-size: 1.1cqw;
    color: #aeb9c9;
    margin-top: 0.6cqw;
  }
  .empty {
    font-size: 1.8cqw;
    color: #b9c1ce;
    line-height: 1.6;
    margin: 3cqw 0;
  }
  .restore {
    font-size: 1.7cqw;
    border-bottom: 1px solid;
    padding-block: 1cqw;
  }
  .pill {
    position: absolute;
    bottom: 1cqw;
    left: 1.3cqw;
    right: 1.3cqw;
    display: flex;
    align-items: center;
    gap: 1cqw;
    padding: 1cqw 1.3cqw;
    min-height: 6.5cqw;
    border-radius: 8cqw;
    background: #252c34d9;
    color: #f2f3f7;
    box-shadow:
      inset 0 1px #ffffff19,
      0 0.5cqw 2cqw #0003;
    backdrop-filter: blur(18px);
    -webkit-backdrop-filter: blur(18px);
  }
  .pill button {
    min-height: 3.5cqw;
    min-width: 2.5cqw;
  }
  .pill-address {
    flex: 1;
    gap: 0.8cqw;
    min-width: 0;
    justify-content: flex-start;
    font-size: 1.25cqw;
  }
  .pill-address span {
    overflow: hidden;
    white-space: nowrap;
    text-overflow: ellipsis;
  }
  .tab-count {
    border: 1px solid #ffffffa0;
    border-radius: 0.6cqw;
    font-size: 1.4cqw;
    height: 2.5cqw;
    min-height: 2.5cqw !important;
  }
  .home {
    padding: 0.8cqw 0 1.1cqw;
    display: flex;
    justify-content: center;
  }
  .home span {
    width: 13cqw;
    height: 0.5cqw;
    border-radius: 10px;
    background: #494b48;
  }
  .android .home span {
    background: #d9dde6;
  }
</style>
