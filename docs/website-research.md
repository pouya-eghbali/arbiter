# Arbiter website research

Reviewed October 25, 2026. This is a qualitative design and positioning review,
not a market-share ranking or a browser performance comparison. Official sites
were read and their desktop homepages inspected in a browser.

## Which alternatives people recommend

Recurring names in the sampled Arc/browser community discussions are Zen,
Vivaldi, Floorp, Orion, Helium, and Dia. Zen is frequently proposed for Arc's
sidebar/workspace feel; the other recommendations depend on whether the person
values customization, simplicity, privacy, platform support, or staying with
The Browser Company. Comments also disagree about reliability and performance;
those anecdotes are not benchmark evidence.

Community sources:

- [ArcBrowser: I need an Arc alternative](https://www.reddit.com/r/ArcBrowser/comments/1v5wrvh/i_need_an_arc_alternative/)
- [ArcBrowser: No alternatives?](https://www.reddit.com/r/ArcBrowser/comments/1vlew8z/no_alternatives/)
- [Browsers: Arc browser replacements so far](https://www.reddit.com/r/browsers/comments/1xd5v20/arc_browser_replacements_so_far/)

The newest discussions also mention many small alpha/beta projects. They are
worth watching but are not treated here as established leaders.

## Website references

### Zen — strongest reference for atmosphere

[zen-browser.app](https://zen-browser.app/)

The homepage leads with an oversized serif headline, cream text on charcoal,
a coral accent, generous empty space, and a small download navigation. Its
positioning combines a calmer experience with privacy. Further down, a large
browser demonstration and four named features explain the product: Workspaces,
Compact Mode, Glance, and Split View. GitHub, documentation, community, and
support links are easy to find.

For Arbiter: borrow the restraint, visual hierarchy, and feeling of calm. Put
our actual product earlier in the first viewport so a visitor understands what
our unusually minimal browser looks like immediately.

### Helium — strongest reference for trust and clarity

[helium.computer](https://helium.computer/)

Uses a restrained blue/indigo palette, direct benefit-led copy, compact download
buttons, and a feature showcase with selectable highlights and a pause control.
The page explains privacy defaults, layouts, extensions, resource use, and open
source. Its lower sections include an interactive browser comparison, migration
reassurance, and detailed FAQs.

For Arbiter: explain privacy concretely, link the source prominently, show how
import works, and answer practical adoption questions. A demonstrable interface
change is more persuasive than an unsupported performance superlative.

### Vivaldi — strongest reference for explaining power

[vivaldi.com](https://vivaldi.com/)

Large sans-serif headline, an immediately recognizable browser screenshot,
bright red download CTA, extensive feature coverage, testimonials, and separate
Powerful / Personal / Private sections. The navigation and full homepage are
much denser than the other references.

For Arbiter: explain workflows and customization with real UI. Keep the homepage
focused on a few differentiators; put the full feature inventory in documentation
or a separate features page.

### Floorp — useful reference for feature explanations and downloads

[floorp.app](https://floorp.app/)

The hero uses a purple/light split composition, clear product screenshot, and
primary download plus secondary feature CTA. Sections explain workspaces,
split view, and web panels through use cases. The comparison table links its
claims to sources, and the download area separates platforms and requirements.

For Arbiter: use concise examples for workspaces and nested splits, clear platform
choices, and documentation links beside advanced features. Comparisons should
have specific definitions, sources, and dates.

### Orion — useful reference for a concise proposition

[orionbrowser.com](https://orionbrowser.com/)

Uses a pale lilac background, purple accents, a three-part feature proposition,
platform-specific downloads, feature cards, and a user-funding explanation.
Its copy makes the engine / extension / privacy combination prominent.

For Arbiter: communicate a small number of concrete strengths. Keep platform
availability clear and put open-source contribution information where it is
visible. Prefer bounded, verifiable privacy statements over absolutes.

### Dia — strongest reference for visual staging and motion

[diabrowser.com](https://www.diabrowser.com/)

A warm yellow textured hero, expressive display typography, a compact floating
navigation, and a large browser demonstration give it a distinctive identity.
Below the hero, numbered use cases and large product scenes explain workflows.
It leads with workday outcomes and AI rather than a feature checklist. Some
embedded media did not play in the inspection browser, reinforcing the need
for useful static fallbacks.

For Arbiter: borrow the deliberate art direction, short demonstrations, and
workflow-based storytelling. Arbiter's story should center on the user's direct
control, focused interface, and privacy.

### Arc — important branding reference, changed homepage

[arc.net](https://arc.net/)

The current homepage is a farewell announcement stating Arc will sunset on
December 22, 2026. It uses a black canvas, a prominent Arc logo, large white
serif typography, a simple CTA, and a long-form explanation. It is no longer
the earlier product landing page. Former paths such as `/students` redirect
to this page.

An older search-indexed version described organization with Spaces and Profiles,
a personal setup using Split View and Themes, and privacy. That older copy was
not treated as the currently rendered homepage or as a verified historical
visual capture.

For Arbiter: the recognizable brand, spacious typography, and personal tone are
useful references. Lead with Arbiter's own identity and demonstrated workflows;
being an Arc alternative can be a secondary discovery page rather than the whole
brand promise.

## Approved direction and implementation

The selected direction is feature-led and image-first, following the product
storytelling of [Dia](https://www.diabrowser.com/), [Ardia](https://ardia.dev/#features),
and [Crest](https://crestbrowser.com/). A warm paper canvas, bold sans-serif
headlines, and yellow/periwinkle stages frame large product demonstrations.
The headline is **The web. On your terms.**

The Svelte implementation lives in [website/](../website/README.md), with
interactive illustrations of the pill, resizable split views, workspaces and
appearance controls. It uses Arbiter's actual logo and the shared Lucide icons.
The illustrations are explicitly identified as sample UI, not browser captures.
Build instructions and release links reflect the current source-first status.
The first version is English; browser UI translations are separate.

The initial dark scenic concept below was not selected.

## Initial recommendation (superseded by the approved direction)

A Glacier-inspired product site: deep ink and soft off-white surfaces, a restrained
cool accent, large editorial typography, and selective tinted glass over beautiful
backgrounds. Use glass where it demonstrates Arbiter, while keeping explanatory
text on stable, readable surfaces.

Proposed hero copy:

> More web. Less browser.
>
> A minimal browser with powerful workspaces, flexible split views, and control
> over every shortcut. Open source. Zero telemetry.

This is proposed positioning, not a claim about competitors. Final release copy
should match the shipped product and published privacy policy.

Suggested homepage sequence:

1. **Hero:** logo and name, one clear promise, OS-specific download plus source
   link, and a large real product scene. Let a short demo reveal the pill and
   create a split so visitors understand the interface.
2. **Focused browsing:** show the full-page view and optional navigation pill.
3. **Room for your workflow:** demonstrate nested splits, resizing, swapping
   panes, and workspaces side by side.
4. **Make it yours:** a restrained interactive light/dark/accent preview, followed
   by customizable shortcuts and portable TOML preferences.
5. **Privacy and ownership:** built-in blocking, automatic private-window locking,
   local credential storage, source code, and GPL v3. Explain distinct protections
   accurately rather than implying complete anonymity.
6. **Make the move:** show browser import and link a short getting-started guide.
7. **Downloads and practical FAQ:** show platform/build status, extension support,
   password/passkey behavior, DRM limitations, contribution links, and release notes.

Keep top navigation small: Features, Downloads, Docs, GitHub. Reuse Arbiter's logo
and Lucide icon system. Provide a language selector and default the site to the
visitor's supported language, consistently with onboarding.

Motion should explain behavior: pill reveal, divider resize, workspace switch,
and theme changes. Provide still images and reduced-motion behavior. Avoid
scroll locking and autoplay sound. Screenshots should show coherent real tasks
and supported capabilities; use static media fallbacks for every demonstration.

The most useful combination is Zen's restraint, Helium's concrete trust messaging,
and Dia's product staging, expressed through Arbiter's own Glacier UI.
