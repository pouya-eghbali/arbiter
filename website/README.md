# Arbiter website

The feature-led Arbiter landing page, built with Svelte 5, SvelteKit 2, TypeScript,
and Vite. It prerenders to static HTML: no server, database, analytics, cookies,
external font requests, or API keys are needed.

## Develop

Use Node.js 22.12+ (or a newer supported LTS version).

```sh
cd website
npm ci
npm run dev
```

Open the local URL printed by Vite. To validate and produce the static site:

```sh
npm run check
npm run build
npm run preview
```

Deploy the contents of **website/build/** to a static host. The page works at a
domain root; for hosting under a subdirectory, configure SvelteKit's base path
and update the root-relative static asset URLs. No hosting account or deployment
configuration is checked in, and building does not publish anything.

## Design and behavior

- Warm paper, yellow and periwinkle product stages follow the approved concept.
- The hero has a keyboard-navigable feature selector. Arrow keys, Home and End
  switch previews; Tab enters the preview controls.
- Navigation pills collapse and expand. Split dividers accept pointer dragging
  and Left/Right/Home/End keys. Pointer capture keeps dragging reliable.
- Workspace selectors and light/dark/accent controls update the illustrations.
- Mobile layouts preserve the page hierarchy, with compact product previews.
- Reduced motion is respected; there are no autoplay videos or scroll locks.
- All navigation, project links, FAQ, text content and illustrations are
  prerendered. Interactive enhancements need JavaScript.

These are **interactive illustrations**, not screenshots or an embedded browser.
Aster Journal and its editorial content are sample material. The wallpaper,
illustrations, and sample websites don't imply partnerships. Feature descriptions
must stay consistent with the browser's README and platform validation status.

There are currently no published installers. Calls to action intentionally lead
to build instructions and the GitHub releases page. Once releases exist, replace
these with verified platform download URLs and retain a source-build option.
The page does not detect visitor OS or make background release API requests.

## Source map

- **src/routes/+page.svelte**: content, navigation, FAQ, project/release links.
- **src/lib/BrowserDemo.svelte**: hero feature selector and appearance controls.
- **src/lib/NavigationPill.svelte**: shared navigation illustration.
- **src/lib/SplitDemo.svelte**: bounded, accessible resizable panes.
- **src/lib/WorkspaceDemo.svelte**: shared workspace illustration.
- **src/lib/Journal.svelte**: sample editorial website.
- **src/app.css**: responsive design and shared visual styles.
- **static/art/**: original SVG artwork, editable without generated raster assets.

## Licenses

Website code and original artwork follow the repository's **GPL-3.0-only**
license. The logo comes from Arbiter's existing application asset.
Lucide SVGs are copied from the browser's existing subset, with notices bundled in
**static/Lucide-LICENSE.txt**. DM Sans is locally bundled through Fontsource under
the SIL Open Font License, with the full notice in **static/DM-Sans-LICENSE.txt**.
The page uses system Georgia for serif sample content. No third-party website
screenshots or generated UI images are shipped.
