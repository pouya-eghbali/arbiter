<script lang="ts">
  import { onDestroy } from 'svelte';
  import Icon from './Icon.svelte';
  import Journal from './Journal.svelte';
  import NavigationPill from './NavigationPill.svelte';
  const categories = [
    'Appearance',
    'Passwords',
    'Passkeys',
    'Keyboard',
    'Configuration',
    'Import',
    'Performance',
    'Privacy',
    'Extensions',
    'Downloads',
    'About',
    'Sync'
  ];
  // Deliberately synthetic, local-only UI state. Never read credentials,
  // clipboard contents, biometrics or network peers from a marketing preview.
  let unlocked = $state(true);
  let background = $state(false);
  let pairing = $state(false);
  let reviewing = $state(false);
  let settingsOpen = $state(true);
  let devices = $state(['iPhone', 'Work PC']);
  let invitation = $state('');
  let synced = $state(false);
  let syncing = $state(false);
  let announcement = $state('Try turning off sync, pairing a device, or syncing the sample devices.');
  let timer: ReturnType<typeof setTimeout> | undefined;
  let connected = $derived(devices.includes('iPhone'));
  const qrCells = Array.from({ length: 441 }, (_, index) => {
    const x = index % 21,
      y = Math.floor(index / 21);
    for (const [left, top] of [
      [0, 0],
      [14, 0],
      [0, 14]
    ]) {
      const dx = x - left,
        dy = y - top;
      if (dx >= 0 && dx < 7 && dy >= 0 && dy < 7)
        return (
          dx === 0 || dx === 6 || dy === 0 || dy === 6 || (dx >= 2 && dx <= 4 && dy >= 2 && dy <= 4)
        );
      if (dx >= -1 && dx <= 7 && dy >= -1 && dy <= 7) return false;
    }
    return (x * 7 + y * 13 + x * y) % 5 < 2;
  });
  function beginPairing() {
    if (!unlocked) return;
    pairing = true;
    reviewing = false;
    announcement = 'Sample pairing code shown. Review the connection on the phone.';
  }
  function connect() {
    if (!connected) devices = [...devices, 'iPhone'];
    pairing = false;
    reviewing = false;
    invitation = '';
    synced = true;
    announcement = 'The sample iPhone is connected. No real devices were paired.';
  }
  function sync() {
    if (!unlocked || syncing) return;
    syncing = true;
    announcement = 'Syncing the sample devices…';
    timer = setTimeout(() => {
      syncing = false;
      synced = true;
      announcement = 'Sample devices are up to date. Nothing was sent over the network.';
    }, 550);
  }
  function lock() {
    clearTimeout(timer);
    unlocked = false;
    syncing = false;
    pairing = false;
    reviewing = false;
    announcement = 'Sync is turned off in the preview.';
  }
  function reset() {
    clearTimeout(timer);
    unlocked = true;
    background = false;
    pairing = false;
    reviewing = false;
    settingsOpen = true;
    devices = ['iPhone', 'Work PC'];
    invitation = '';
    synced = false;
    syncing = false;
    announcement =
      'Preview reset. Try turning off sync, pairing a device, or syncing the sample devices.';
  }
  onDestroy(() => clearTimeout(timer));
</script>

<div class="sync-demo" aria-label="Interactive local sync preview">
  <div class="desktop">
    {#if settingsOpen}
      <aside class="sidebar" aria-label="Example settings categories">
        <strong>Settings</strong>
        <div class="categories">
          {#each categories as category}<span class:chosen={category === 'Sync'}>{category}</span
            >{/each}
        </div>
        <button
          class="done"
          onclick={() => {
            settingsOpen = false;
            announcement = 'Settings closed. Use Reset preview below to return to Sync.';
          }}>Done</button
        >
      </aside>
      <!-- svelte-ignore a11y_no_noninteractive_tabindex (Scrollable settings need a keyboard focus target.) -->
      <div
        class="settings-content"
        role="region"
        tabindex="0"
        aria-label="Scroll desktop Sync settings preview"
      >
        <h3>Sync</h3>
        <h4>Bring your vault along</h4>
        <p class="intro">
          Connect your computers and phones to share one credential vault over local Wi-Fi.
        </p>
        {#if unlocked}
          <div class="unlock-status">
            <Icon name="shield-check" size={18} /><strong>Sync is enabled</strong><button
              onclick={lock}>Turn off</button
            >
          </div>
          <p class="session-note">
            Enable once. Sync starts automatically when Arbiter opens, including after a restart.
            Keep both devices on the same Wi-Fi.
          </p>
          <button class="primary" onclick={beginPairing}>Add a device</button>
          {#if pairing}
            <div class="pairing-code">
              <svg
                viewBox="-2 -2 25 25"
                role="img"
                aria-label="Illustrative pairing QR code, not a real invitation"
                ><rect
                  x="-2"
                  y="-2"
                  width="25"
                  height="25"
                  fill="white"
                />{#each qrCells as cell, index}{#if cell}<rect
                      x={index % 21}
                      y={Math.floor(index / 21)}
                      width="1"
                      height="1"
                      fill="#151b23"
                    />{/if}{/each}</svg
              >
              <span>Sample code · Preview only</span><button
                onclick={() => {
                  pairing = false;
                  reviewing = false;
                  announcement = 'Preview pairing cancelled.';
                }}>Cancel pairing</button
              >
            </div>
          {/if}
          <form
            class="join"
            onsubmit={(e) => {
              e.preventDefault();
              if (invitation.trim()) {
                pairing = true;
                reviewing = true;
                announcement = 'Review the sample connection on the phone.';
              }
            }}
          >
            <input
              aria-label="Sample desktop pairing code"
              bind:value={invitation}
              placeholder="Paste a pairing code from another computer"
              autocomplete="off"
              spellcheck="false"
            />
            <button disabled={!invitation.trim()}>Review connection</button>
          </form>
          <button class="sync-now" disabled={syncing} onclick={sync}
            ><span class:spinning={syncing}><Icon name="rotate-cw" size={14} /></span>{syncing
              ? 'Syncing…'
              : 'Sync now'}</button
          >
          <h5>Paired devices</h5>
          <div class="device-list">
            {#each devices as device (device)}<div class="device">
                <Icon
                  name={device === 'iPhone' ? 'smartphone' : 'panels-top-left'}
                  size={18}
                /><span
                  ><strong>{device}</strong><small
                    >{synced ? 'Last synced just now' : 'Last synced 2 minutes ago'}</small
                  ></span
                ><button
                  aria-label="Remove {device} from the sync preview"
                  onclick={() => {
                    devices = devices.filter((d) => d !== device);
                    announcement = `${device} removed from the preview.`;
                  }}><Icon name="trash" size={13} /></button
                >
              </div>{/each}
          </div>
          {#if !devices.length}<p class="session-note">
              Your connected computers and phones will appear here.
            </p>{/if}
        {:else}
          <div class="locked">
            <Icon name="lock" size={28} />
            <p>Enable encrypted sync for your paired devices.</p>
            <button
              class="primary"
              onclick={() => {
                unlocked = true;
                announcement = 'Sync enabled in this preview. No real credentials are accessed.';
              }}>Enable sync</button
            >
          </div>
        {/if}
      </div>
    {:else}
      <div class="closed-settings">
        <Journal compact />
        <div class="browser-pill"><NavigationPill /></div>
      </div>
    {/if}
  </div>
  <section class="phone" aria-label="Mobile Sync settings preview">
    <div class="phone-status">
      <span>9:41</span><span class="phone-camera"></span><span aria-hidden="true">▰</span>
    </div>
    <!-- svelte-ignore a11y_no_noninteractive_tabindex (Scrollable settings need a keyboard focus target.) -->
    <div
      class="phone-scroll"
      role="region"
      tabindex="0"
      aria-label="Scroll mobile Sync settings preview"
    >
      <div class="phone-title">
        <Icon name="arrow-left" size={14} />
        <h3>Sync</h3>
        <button
          aria-label="Close mobile pairing review"
          disabled={!pairing}
          onclick={() => {
            pairing = false;
            reviewing = false;
            announcement = 'Preview pairing cancelled.';
          }}><Icon name="x" size={13} /></button
        >
      </div>
      {#if pairing}
        <h4>{reviewing ? 'Review connection' : 'Scan desktop code'}</h4>
        <p>Connect to MacBook?</p>
        <p class="phone-note">This shares your credential vault with the connected computer.</p>
        {#if reviewing}<div class="phone-group">
            <strong>MacBook</strong><small>Sample connection</small><button
              class="primary"
              onclick={connect}>Connect</button
            ><button
              onclick={() => {
                pairing = false;
                reviewing = false;
              }}>Cancel</button
            >
          </div>
        {:else}<div class="scan-frame">
            <Icon name="qr-code" size={34} /><span>Sample pairing preview</span>
          </div>
          <button
            class="phone-button"
            onclick={() => {
              reviewing = true;
              announcement = 'Review the sample MacBook connection.';
            }}>Review connection</button
          >{/if}
      {:else}
        <h4>Local encrypted sync</h4>
        <div class="phone-group">
          <p>{connected ? 'Paired with MacBook' : 'No paired desktop'}</p>
          <label class="checkbox"
            ><input type="checkbox" bind:checked={background} /><span>Allow background sync</span
            ></label
          ><button
            class="phone-button"
            disabled={!unlocked || syncing}
            onclick={() => (connected ? sync() : beginPairing())}
            >{syncing ? 'Syncing…' : connected ? 'Sync now' : 'Pair with your desktop'}</button
          ><button class="phone-button" disabled={!unlocked} onclick={beginPairing}
            ><Icon name="plus" size={12} />Add another computer</button
          >
          {#if connected}<div class="phone-device">
              <Icon name="smartphone" size={14} /><span
                ><strong>MacBook</strong><small
                  >{synced ? 'Last synced just now' : 'Last synced 2 minutes ago'}</small
                ></span
              >
            </div>{/if}
        </div>
        <p class="phone-note">
          Direct encrypted sync over the same Wi-Fi. Enable sync on desktop and keep Arbiter open.
        </p>
      {/if}
    </div>
    <span class="home"></span>
  </section>
</div>
<div class="demo-caption">
  <span>INTERACTIVE PREVIEW · SAMPLE DEVICES</span><button onclick={reset}
    >Reset preview <Icon name="rotate-cw" size={12} /></button
  >
</div>
<p class="demo-status" role="status">{announcement}</p>

<style>
  .sync-demo {
    container-type: inline-size;
    color-scheme: dark;
    position: relative;
    aspect-ratio: 1.18;
    border-radius: 18px;
    isolation: isolate;
    background: #e9e3d5;
    overflow: clip;
    color: #edf0f6;
  }
  .sync-demo::before {
    content: '';
    position: absolute;
    width: 74%;
    height: 110%;
    left: -9%;
    top: -10%;
    background: #afb6a1;
    transform: rotate(-22deg);
    z-index: -1;
  }
  .sync-demo::after {
    content: '';
    position: absolute;
    width: 70%;
    height: 22%;
    bottom: -5%;
    right: -10%;
    background: #e8c665;
    transform: rotate(-12deg);
    z-index: -1;
  }
  button {
    display: inline-flex;
    align-items: center;
    justify-content: center;
    gap: 0.8cqw;
    font-size: 1.45cqw;
    padding: 1cqw 1.6cqw;
    border-radius: 1cqw;
    background: #ffffff13;
    color: inherit;
    min-height: 4cqw;
    line-height: 1.25;
  }
  button:hover:not(:disabled) {
    background: #ffffff25;
  }
  button:disabled {
    opacity: 0.45;
  }
  .primary {
    background: #b6a0fa;
    color: #111622;
    font-weight: 600;
  }
  .primary:hover:not(:disabled) {
    background: #cab8ff;
  }
  button:focus-visible,
  input:focus-visible {
    outline-width: 2px;
    outline-offset: 2px;
  }
  .desktop {
    position: absolute;
    left: 21%;
    right: 3%;
    top: 6%;
    bottom: 5%;
    display: grid;
    grid-template-columns: 27% minmax(0, 1fr);
    background: #131c27;
    border: 1px solid #56606a;
    border-radius: 2.1cqw;
    box-shadow: 0 1.5cqw 3cqw #1c242b40;
    overflow: hidden;
  }
  .sidebar {
    display: flex;
    flex-direction: column;
    padding: 2.5cqw 1cqw 2cqw 1.7cqw;
    border-right: 1px solid #ffffff13;
    min-height: 0;
  }
  .sidebar > strong {
    font-size: 1.9cqw;
    font-weight: 600;
    margin: 0 0 2cqw 0.8cqw;
  }
  .categories {
    overflow: auto;
    flex: 1;
    min-height: 0;
    scrollbar-width: thin;
  }
  .categories span {
    display: block;
    padding: 1.05cqw 1cqw;
    border-radius: 0.9cqw;
    font-size: 1.35cqw;
    line-height: 1.2;
    color: #c0c7d3;
  }
  .categories .chosen {
    background: #b19af52c;
    color: #f1edff;
  }
  .done {
    width: 100%;
    margin-top: 1.5cqw;
    background: none;
    border: 1px solid #ffffff30;
  }
  .settings-content {
    padding: 2.5cqw 2.7cqw;
    overflow: auto;
    overscroll-behavior: contain;
    scrollbar-width: thin;
    min-width: 0;
  }
  .settings-content h3 {
    font-size: 2.3cqw;
    font-weight: 600;
    margin-bottom: 2.2cqw;
  }
  .settings-content h4 {
    font-size: 2.2cqw;
    font-weight: 600;
    letter-spacing: -0.04cqw;
  }
  .intro {
    font-size: 1.45cqw;
    line-height: 1.5;
    color: #bbc4d0;
    margin-top: 1.1cqw;
  }
  .checkbox {
    display: flex;
    align-items: center;
    gap: 1.1cqw;
    font-size: 1.35cqw;
    line-height: 1.5;
    margin-block: 1.8cqw;
  }
  .checkbox input {
    appearance: auto;
    flex: 0 0 auto;
    width: 2.1cqw;
    height: 2.1cqw;
    padding: 0;
    margin: 0;
    accent-color: #b6a0fa;
  }
  .unlock-status {
    display: flex;
    align-items: center;
    gap: 1.2cqw;
    margin-block: 1.8cqw 0.7cqw;
    font-size: 1.6cqw;
  }
  .unlock-status strong {
    font-weight: 500;
  }
  .unlock-status button {
    margin-left: auto;
    min-height: 3cqw;
    background: none;
    padding: 0.5cqw;
    color: #c2b0f8;
  }
  .session-note {
    font-size: 1.2cqw;
    color: #b8c0ce;
    line-height: 1.5;
    margin-bottom: 1.8cqw;
  }
  .join {
    display: grid;
    justify-items: start;
    gap: 1cqw;
    margin-top: 1.2cqw;
  }
  .join input {
    width: 100%;
    min-width: 0;
    border: 1px solid #87929e;
    border-radius: 0.9cqw;
    background: transparent;
    color: #f0f2f8;
    padding: 1.2cqw;
    font-size: 1.25cqw;
    line-height: 1.4;
  }
  .join input::placeholder {
    color: #b0b9c6;
  }
  .sync-now {
    margin-block: 1cqw;
    background: none;
    padding-inline: 0;
  }
  .settings-content h5 {
    font-size: 1.8cqw;
    font-weight: 600;
    margin: 1cqw 0;
  }
  .device {
    display: flex;
    align-items: center;
    gap: 1.3cqw;
    padding-block: 1.2cqw;
  }
  .device > span {
    flex: 1;
    min-width: 0;
  }
  .device strong {
    font-size: 1.5cqw;
    font-weight: 500;
    display: block;
  }
  .device small {
    display: block;
    font-size: 1.15cqw;
    color: #b8c0cd;
    margin-top: 0.5cqw;
  }
  .device button {
    padding: 0.7cqw;
    min-height: 3cqw;
    background: none;
  }
  .pairing-code {
    padding: 1.8cqw 0;
    display: grid;
    gap: 1cqw;
    justify-items: start;
  }
  .pairing-code svg {
    width: 17cqw;
    height: 17cqw;
    border-radius: 0.7cqw;
  }
  .pairing-code > span {
    font-size: 1.3cqw;
    color: #b7c1ce;
  }
  .locked {
    display: grid;
    justify-items: start;
    gap: 2cqw;
    padding-top: 4cqw;
  }
  .locked p {
    font-size: 1.7cqw;
  }

  .closed-settings {
    grid-column: 1 / -1;
    position: relative;
    min-width: 0;
    height: 100%;
  }
  .browser-pill {
    position: absolute;
    bottom: 4%;
    left: 10%;
    right: 10%;
  }
  .browser-pill :global(.navigation-pill) {
    width: 100%;
    height: 4.8cqw;
  }
  .browser-pill :global(.pill-content) {
    gap: 1cqw;
  }
  .browser-pill :global(.pill-address) {
    font-size: 1.3cqw;
  }
  .phone {
    position: absolute;
    left: 1.5%;
    width: 26%;
    aspect-ratio: 390 / 844;
    bottom: 2%;
    border: 0.6cqw solid #1c1e23;
    border-radius: 4.4cqw;
    background: #141e29;
    box-shadow:
      0 1.3cqw 2cqw #1a1c2540,
      inset 0 0 0 1px #ffffff60;
    display: flex;
    flex-direction: column;
    overflow: hidden;
  }
  .phone-status {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 1.6cqw 1.8cqw 1cqw;
    font-size: 1cqw;
    min-height: 4.5cqw;
  }
  .phone-camera {
    width: 7cqw;
    height: 1.6cqw;
    border-radius: 10px;
    background: #03080d;
  }
  .phone-scroll {
    flex: 1;
    min-height: 0;
    padding: 1cqw 1.5cqw 3cqw;
    overflow: auto;
    overscroll-behavior: contain;
    scrollbar-width: thin;
  }
  .phone-title {
    display: flex;
    align-items: center;
    gap: 1cqw;
    margin: 1cqw 0 3cqw;
  }
  .phone-title h3 {
    font-size: 1.9cqw;
    font-weight: 650;
    flex: 1;
  }
  .phone-title button {
    padding: 0;
    min-height: 2.5cqw;
    background: none;
  }
  .phone-title button:disabled {
    opacity: 0.45;
  }
  .phone h4 {
    font-size: 1.55cqw;
    color: #c1c8d3;
    margin-bottom: 1.7cqw;
  }
  .phone p {
    font-size: 1.35cqw;
    color: #b8c1d0;
    line-height: 1.5;
  }
  .phone-group {
    padding: 1.6cqw 1.3cqw;
    background: #242e3a;
    border-radius: 2.5cqw;
  }
  .phone-group > strong {
    display: block;
    font-size: 1.6cqw;
  }
  .phone-group > small {
    display: block;
    font-size: 1.2cqw;
    margin-top: 0.7cqw;
    color: #bbc5d4;
  }
  .phone-group .checkbox {
    margin-block: 2.2cqw;
    font-size: 1.2cqw;
    gap: 0.9cqw;
  }
  .phone-group .checkbox input {
    width: 1.8cqw;
    height: 1.8cqw;
  }
  .phone-button,
  .phone-group > button {
    width: 100%;
    margin-top: 1.1cqw;
    padding: 1.2cqw 0.6cqw;
    font-size: 1.25cqw;
    min-height: 4.6cqw;
    border-radius: 1.2cqw;
  }
  .phone-device {
    display: flex;
    align-items: center;
    gap: 1cqw;
    margin-top: 2cqw;
    border-top: 1px solid #ffffff12;
    padding-top: 1.5cqw;
  }
  .phone-device strong {
    font-size: 1.3cqw;
    display: block;
    font-weight: 500;
  }
  .phone-device small {
    font-size: 1cqw;
    display: block;
    margin-top: 0.5cqw;
    color: #b5c1d2;
  }
  .phone .phone-note {
    font-size: 1.2cqw;
    margin-top: 2cqw;
  }
  .home {
    width: 9cqw;
    height: 0.35cqw;
    margin: 0 auto 1cqw;
    background: #aeb9c8;
    border-radius: 10px;
  }
  .scan-frame {
    display: grid;
    justify-items: center;
    gap: 2cqw;
    border: 1px solid #ffffff35;
    padding: 3cqw;
    border-radius: 2cqw;
    margin-top: 2cqw;
  }
  .scan-frame span {
    font-size: 1.25cqw;
    color: #b6c3d2;
  }
  .demo-caption {
    display: flex;
    flex-wrap: wrap;
    justify-content: space-between;
    gap: 10px;
    align-items: center;
    margin-top: 17px;
    font-size: 8px;
    letter-spacing: 0.7px;
    color: #5d5872;
  }
  .demo-caption button {
    color: #272332;
    background: transparent;
    font-size: 11px;
    padding: 6px 0;
    min-height: 32px;
    gap: 7px;
  }
  .demo-caption button:hover:not(:disabled) {
    text-decoration: underline;
    background: none;
  }
  .demo-status {
    min-height: 2.8em;
    font-size: 11px;
    line-height: 1.5;
    color: #5d5872;
    margin-top: 5px;
  }
  .spinning {
    display: inline-flex;
    animation: spin 1s linear infinite;
  }
  @keyframes spin {
    to {
      transform: rotate(360deg);
    }
  }
  @media (prefers-reduced-motion: reduce) {
    .spinning {
      animation: none;
    }
  }
</style>
