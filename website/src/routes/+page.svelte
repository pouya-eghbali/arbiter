<script lang="ts">
  import BrowserDemo from '$lib/BrowserDemo.svelte';
  import NavigationPill from '$lib/NavigationPill.svelte';
  import SplitDemo from '$lib/SplitDemo.svelte';
  import WorkspaceDemo from '$lib/WorkspaceDemo.svelte';
  import Icon from '$lib/Icon.svelte';
  const repo = 'https://github.com/pouya-eghbali/arbiter';
  const downloads = repo + '/releases/download/v0.1.0/';
  let tucked = $state(false);
  const extras = [
    {
      icon: 'shield-check',
      title: 'Less noise, built in.',
      description: 'Ad blocking with a quick toggle for the page you’re on.'
    },
    {
      icon: 'key-round',
      title: 'Keep your keys close.',
      description: 'Passwords in your OS credential store. Local passkeys, too.'
    },
    {
      icon: 'eye-off',
      title: 'Private means private.',
      description: 'Private windows that can blur, darken, and lock when you step away.'
    },
    {
      icon: 'puzzle',
      title: 'Bring your favorites.',
      description: 'Install extensions from the Chrome Web Store. Compatibility varies.'
    },
    {
      icon: 'code-xml',
      title: 'Get under the hood.',
      description: 'Chromium DevTools, custom shortcuts, and settings in TOML.'
    },
    {
      icon: 'layers',
      title: 'Let idle tabs rest.',
      description: 'Automatic tab sleeping keeps visible views and audio awake.'
    }
  ];
</script>

<svelte:head>
  <title>Arbiter — The web. On your terms.</title>
  <meta
    name="description"
    content="A browser for power users. A floating navigation pill, flexible split views, and workspaces. Open source, zero telemetry, and yours to customize."
  />
  <meta property="og:title" content="Arbiter — The web. On your terms." />
  <meta
    property="og:description"
    content="Less in the way. More room for your web. Meet the open-source browser built around your workflow."
  />
  <meta property="og:type" content="website" />
</svelte:head>

<a class="skip-link" href="#main">Skip to content</a>
<header class="site-header wrap">
  <a href="#top" class="brand" aria-label="Arbiter home"
    ><span class="brand-mark"><img src="/arbiter.png" alt="" width="36" height="36" /></span
    >Arbiter</a
  >
  <nav aria-label="Main navigation">
    <a href="#features">Features</a><a href="#get-arbiter">Get Arbiter</a><a href={repo}
      >GitHub <Icon name="external-link" size={12} /></a
    >
  </nav>
  <a href="#get-arbiter" class="button button-small"
    ><Icon name="download" size={16} /><span>Get Arbiter</span></a
  >
</header>

<main id="main">
  <section class="hero wrap" id="top" aria-labelledby="hero-title">
    <div>
      <p class="eyebrow"><span class="status-dot"></span>A LITTLE LESS BROWSER. A LOT MORE WEB.</p>
      <h1 id="hero-title">The web.<br />On your terms<span class="period">.</span></h1>
      <p class="hero-description">
        An open-source browser for people<br class="desktop-break" /> who make the web their own.
      </p>
      <div class="hero-actions">
        <a class="button" href="#get-arbiter"><Icon name="download" size={19} />Get Arbiter</a><a
          class="text-link"
          href={repo}>Explore the source <Icon name="arrow-right" size={17} /></a
        >
      </div>
      <p class="platforms">Available for macOS & Linux. Windows coming soon.</p>
    </div>
    <div class="hero-aside">
      <span class="hero-asterisk" aria-hidden="true">✳</span>
      <p>Your pages take the stage.<br />Your browser knows when<br />to step aside.</p>
      <a href="#preview">See for yourself <span>↓</span></a>
    </div>
  </section>

  <section class="preview-section" id="preview" aria-label="Interactive Arbiter product preview">
    <div class="wrap"><BrowserDemo /></div>
  </section>

  <section class="feature-row wrap pill-section" id="features" aria-labelledby="pill-title">
    <div class="feature-copy">
      <p class="eyebrow">01 / LESS IN THE WAY</p>
      <h2 id="pill-title">There when <br />you need it.</h2>
      <p>A floating pill. A shortcut away.<br />The rest is your page.</p>
      <button class="text-link" aria-pressed={tucked} onclick={() => (tucked = !tucked)}
        >{tucked ? 'Bring it back' : 'Watch it step aside'}<Icon
          name={tucked ? 'plus' : 'arrow-right'}
          size={17}
        /></button
      >
    </div>
    <div class="pill-art-stage">
      <span class="stage-note">JUST ENOUGH BROWSER.</span><NavigationPill
        collapsed={tucked}
        large
      /><span class="stage-caption"
        >{tucked ? 'Out of the way. Never out of reach.' : 'Everything you need, right here.'}</span
      >
    </div>
  </section>

  <section class="split-section" aria-labelledby="split-title">
    <div class="feature-row wrap">
      <div class="feature-copy">
        <p class="eyebrow">02 / MORE SIDE BY SIDE</p>
        <h2 id="split-title">Split your view. <br />Keep your flow.</h2>
        <p>Two, three, or more. Keep what you need in view, and work at your own pace.</p>
        <p class="feature-detail">
          Split horizontally or vertically. Resize freely. Swap panes without losing your place.
        </p>
        <span class="interaction-hint"
          ><Icon name="columns-2" size={17} />Try dragging the divider <span>↗</span></span
        >
      </div>
      <div class="split-visual">
        <SplitDemo />
        <div class="visual-label"><span class="tiny-dot"></span>ONE WINDOW. YOUR ARRANGEMENT.</div>
      </div>
    </div>
  </section>

  <section class="feature-row wrap workspace-section" aria-labelledby="workspace-title">
    <div class="feature-copy">
      <p class="eyebrow">03 / ROOM FOR EVERY CONTEXT</p>
      <h2 id="workspace-title">Work. Play. <br />A little of both.</h2>
      <p>A space for each part of your day. Separate sessions when you need them.</p>
      <p class="feature-detail">
        Bring workspaces side by side in split view. Your shortcuts follow the workspace you’re in.
      </p>
      <span class="interaction-hint"
        ><Icon name="layers" size={17} />Try switching spaces <span>↗</span></span
      >
    </div>
    <div class="workspace-art-stage">
      <div class="workspace-art-block"></div>
      <WorkspaceDemo /><span class="stage-note workspace-stage-note"
        >SAME BROWSER. DIFFERENT HEADSPACE.</span
      >
    </div>
  </section>

  <section class="more-section wrap" aria-labelledby="more-title">
    <div class="section-heading">
      <p class="eyebrow">THE DETAILS MAKE THE DIFFERENCE</p>
      <h2 id="more-title">Quietly capable.</h2>
      <p>The everyday things, thought through.</p>
    </div>
    <div class="extras">
      {#each extras as extra}<article>
          <span class="extra-icon"><Icon name={extra.icon} size={24} /></span>
          <div>
            <h3>{extra.title}</h3>
            <p>{extra.description}</p>
          </div>
        </article>{/each}
    </div>
  </section>

  <section class="open-source-section" aria-labelledby="open-title">
    <div class="wrap open-source-content">
      <div>
        <p class="eyebrow">BUILT IN THE OPEN</p>
        <h2 id="open-title">Your browser.<br />Nobody’s business.</h2>
        <p>
          Open source. Zero telemetry. No data collection.<br />Made for power users, open to
          everyone.
        </p>
        <a class="button button-cream" href={repo}
          >Make it better with us <Icon name="arrow-right" size={18} /></a
        >
      </div>
      <div class="source-note">
        <span class="source-note-top"
          >A NOTE FROM THE PROJECT <Icon name="code-xml" size={18} /></span
        >
        <p>Good software should belong to the people who use it.</p>
        <p>
          Arbiter is free software under the GNU GPL v3. Read the code, make it your own, and help
          shape what comes next.
        </p>
        <a href={repo + '/issues'} class="contribute-link"
          >Ideas, issues, and pull requests welcome <Icon name="arrow-right" size={16} /></a
        >
      </div>
    </div>
  </section>

  <section class="get-section wrap" id="get-arbiter" aria-labelledby="get-title">
    <div class="section-heading">
      <p class="eyebrow"><span class="status-dot"></span>EARLY DAYS. OPEN DOORS.</p>
      <h2 id="get-title">Make yourself<br />at home.</h2>
      <p>Arbiter is in active development.<br />Come explore, experiment, and build with us.</p>
    </div>
    <div class="get-options">
      <a class="get-option primary" href={downloads + 'arbiter-0.1.0-macos-arm64.dmg'}
        ><span
          ><Icon name="download" size={27} /><strong>Download for macOS</strong><small
            >Apple Silicon · macOS 14.4 or later.</small
          ></span
        ><Icon name="arrow-right" size={25} /></a
      ><a class="get-option" href={downloads + 'arbiter-0.1.0-linux-x86_64.tar.xz'}
        ><span
          ><Icon name="download" size={27} /><strong>Download for Linux</strong><small
            >x86_64 · Ubuntu 24.04 or compatible.</small
          ></span
        ><Icon name="arrow-right" size={22} /></a
      >
      <p class="build-note">
        Windows coming soon. <a href={repo + '/releases'}>Release notes & other downloads</a>.
      </p>
    </div>
  </section>

  <section class="faq-section wrap" aria-labelledby="faq-title">
    <h2 id="faq-title">A few good questions.</h2>
    <div class="faq-items">
      <details>
        <summary>Can I bring my passwords and history?<Icon name="plus" size={18} /></summary>
        <p>
          Yes. Setup and Settings include browser import for available passwords and history. What
          can be imported depends on the source browser, operating system, and permissions. Password
          files can also be imported in Settings.
        </p>
      </details>
      <details>
        <summary>Can I make it look like me?<Icon name="plus" size={18} /></summary>
        <p>
          Choose system, light, or dark appearance. Set separate accent colors, background colors,
          and background pictures for light and dark modes. Choose a visible pill handle, an
          invisible reveal zone, or shortcut-only navigation. Keyboard shortcuts are customizable,
          too. Try the “Make it yours” preview above.
        </p>
      </details>
      <details>
        <summary>Does Arbiter collect my data?<Icon name="plus" size={18} /></summary>
        <p>
          No telemetry or data collection is built into Arbiter. This website also uses no
          analytics, tracking, cookies, or third-party fonts. Browsing still contacts the sites you
          visit; enabled features such as favicon loading and ad-block filter updates can make
          network requests.
        </p>
      </details>
      <details>
        <summary>What about streaming and DRM?<Icon name="plus" size={18} /></summary>
        <p>
          Arbiter does not bundle or download Widevine. When available, it can use a compatible
          Widevine module already installed on your device. Widevine is proprietary Google software
          governed by applicable Google license terms. Compatibility with streaming services and
          playback quality are not guaranteed.
        </p>
      </details>
      <details>
        <summary>Is the preview the actual browser?<Icon name="plus" size={18} /></summary>
        <p>
          The previews on this page are interactive illustrations of Arbiter’s features, with sample
          pages and content. Arbiter itself is a native C++ and Qt WebEngine desktop browser. See
          the repository for implementation details and current platform limitations.
        </p>
      </details>
    </div>
  </section>
</main>

<footer class="site-footer wrap">
  <a class="brand" href="#top"
    ><span class="brand-mark"><img src="/arbiter.png" alt="" width="32" height="32" /></span
    >Arbiter</a
  ><span>A little more room for your web.</span>
  <nav aria-label="Project links">
    <a href={repo}>GitHub</a><a href={repo + '/issues'}>Report an issue</a><a
      href={repo + '/blob/master/LICENSE'}>GPL v3</a
    >
  </nav>
</footer>
