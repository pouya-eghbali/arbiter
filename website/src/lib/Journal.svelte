<script lang="ts">
  let { compact = false, alternate = false }: { compact?: boolean; alternate?: boolean } = $props();
</script>

<div class="journal" class:compact>
  <div class="journal-nav">
    <span class="aster">Aster<span>✳</span></span><span>Journal</span><span>Editions</span><span
      >About</span
    ><span class="journal-end">An independent collection</span>
  </div>
  <div class="journal-main">
    <div class="journal-lead">
      <span class="journal-kicker">A JOURNAL OF EVERYDAY DISCOVERIES</span>
      <h3>{alternate ? 'A slower kind of Sunday.' : 'A more human web.'}</h3>
      <p>Thoughts, tools and objects for a calmer, more creative internet.</p>
      <span class="read-story">Read the latest <span>↗</span></span>
      <span class="edition">ISSUE 004 — THE EVERYDAY</span>
    </div>
    <img
      class="journal-art"
      src="/art/geometry.svg"
      alt="Orange geometric art print with a red sun and architectural shapes"
      width="600"
      height="850"
    />
    <div class="journal-side">
      <p class="journal-intro">A journal of ideas, objects and a more considered internet.</p>
      <div class="journal-story">
        <img
          src="/art/botanical.svg"
          alt="Illustration of an olive-green houseplant in a ceramic pot"
          width="500"
          height="400"
        />
        <div>
          <h4>Slower software,<br />brighter days</h4>
          <p>On tools that respect our attention.</p>
        </div>
      </div>
      <div class="journal-story">
        <img
          src="/art/chair.svg"
          alt="Illustration of an orange modernist chair"
          width="500"
          height="400"
        />
        <div>
          <h4>The objects<br />around us</h4>
          <p>A small collection for a fuller life.</p>
        </div>
      </div>
      <span class="journal-footnote">LESS NOISE. MORE CURIOSITY.</span>
    </div>
  </div>
</div>
