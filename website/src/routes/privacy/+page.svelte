<script lang="ts">
  import policy from '../../../../docs/privacy-policy.md?raw';
  // Policy source is shared with the offline mobile copy. Plain paragraphs avoid
  // introducing an HTML/Markdown parser into this static document.
  const sections = policy.trim().split(/\n\s*\n/);
</script>

<svelte:head><title>Privacy policy — Arbiter</title><meta name="description" content="How Arbiter handles browsing data, credentials, permissions and local encrypted sync." /></svelte:head>
<main>
  <a href="/">← Arbiter</a>
  {#each sections as section}
    {#if section.startsWith('# ')}<h1>{section.slice(2)}</h1>
    {:else if section.startsWith('## ')}<h2>{section.slice(3)}</h2>
    {:else}<p>{section.replace(/\n/g, ' ')}</p>{/if}
  {/each}
</main>
<style>
  main { max-width: 48rem; margin: 0 auto; padding: 3rem 1.5rem 6rem; color: #222; }
  h1 { margin: 2rem 0; font-size: clamp(2rem, 5vw, 3.5rem); line-height: 1.1; }
  h2 { margin: 2.5rem 0 1rem; font-size: 1.5rem; }
  p { line-height: 1.8; margin: 1rem 0; overflow-wrap: anywhere; }
  a { color: #4530a3; text-decoration: underline; }
</style>
