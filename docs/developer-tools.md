# Developer tools

Open the focused tab's Chromium developer tools with **Cmd+Option+I** on macOS or
**Ctrl+Shift+I** on Windows/Linux. Change the binding under Settings → Keyboard →
Toggle developer tools for focused view. The navigation pill's More menu also has
Developer tools; right-click a page and choose Inspect element to select that
location in the Elements panel.

Tools dock below the page, with a draggable top divider. Double-click the divider
to restore its default height. The page and all split panes resize into the
remaining area. The dock uses Chromium’s own toolbar and close button. Elements/styles, Console,
Sources/debugging, Network and the other panels are the bundled Chromium frontend.

Switching tabs or focusing another split pane leaves the debugger attached to the
original tab so ongoing inspection is preserved. Invoke the shortcut or menu item
again to inspect the newly focused tab. On the already inspected tab, the shortcut
toggles tools closed. Chromium’s close button also closes the dock.

Inspected tabs stay awake even when hidden. Closing their tab, entering Settings,
locking private browsing or shutting down detaches and destroys the frontend.
Private tools live inside the browser window, covered by its existing privacy
shield; there is no separate window with private debugging data left visible.
The frontend has its own off-the-record profile and receives no Arbiter credential
bridge, user scripts, extensions or persistent browser-profile data. Normal
inspection still exposes the inspected page's data to its user, as expected.

Implementation uses Qt's public `WebEngineView.inspectedView` and native
`InspectElement` action. No remote debugging port, engine patch or external
DevTools server is needed. A new frontend is created on reopening; its UI session
is not persisted.

## Validation

```sh
cmake --build build-patched-tests --target devtools_test keybindings_test --parallel 24
ctest --test-dir build-patched-tests -R '^(devtools|keybindings)$' --output-on-failure
```

The test loads the actual dock and bundled frontend with a temporary page, executes
JavaScript through its DevTools runtime agent and verifies the result in that page.
It checks off-the-record frontend isolation, resizing, retargeting, detachment and
frontend removal. The browser UI harness additionally checks split targeting,
inspected-tab closure, private-lock/settings teardown, keyboard toggling, actual
resize dragging and the native context-menu inspect path.

Runtime verification is on macOS. Windows/Linux use the same Qt APIs; runtime
validation on those platforms remains pending.

## Page context menu

Right-click uses Arbiter's themed glass menu, with a compact icon-and-label strip
for common actions. It shows navigation on ordinary pages, cut/copy/paste in text
fields, copy for a selection, image copy/save, or playback controls for media.
Regular rows have aligned Lucide icons, separators between context/site/extension/
developer actions, and shortcut hints; Print and Saved passwords follow customized
bindings. Text editing stays with Chromium's native actions and edit flags.

The action strip supports left/right selection and Enter/Space activation. Clicking
empty strip space does not execute an action. Menus keep native disabled states,
spell-check suggestions, extension check/radio state, and the password-field probe.
The menu closes on navigation so it cannot act on a different document.
