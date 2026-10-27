# Release notes

`docs/release-notes.md` is the release document for the current source revision.
Update it whenever publishing component changes. The preview workflow requires
that file to change with a component push. Each immutable source tag preserves
its own notes; the GitHub release publishes the same content and a Markdown asset.

In the launcher, open About and choose **What's new in this build**, or choose
**Read update notes** after a newer release is found. The built-in viewer renders
headings, paragraphs, **bold**, *emphasis*, lists and inline PNG/JPEG images. Notes
for a pending update come from its verified source revision, not the latest branch.
The installed document is available offline. Missing optional images do not hide
the text. Old previews without a document report that notes are unavailable.

## Authoring

Start with a level-one title, group user-visible changes under concise headings,
and explain behavior instead of internal activity. Include material limitations.
For example:

```markdown
# Launcher preview

## New

  * **Release notes.** Read changes inside the launcher before updating.

![Campaign view](images/campaign-view.png)
```

Keep notes below 64 KiB UTF-8. Images must use relative `images/...` paths under
`docs/`; up to eight distinct PNG/JPEG images are loaded, each at most 4 MiB and
2048 pixels on either axis. External URLs, traversal paths and arbitrary HTML are
not loaded. Image sizes scale to the available viewer width. Links are displayed
as text without launching an external application. Tables, embedded videos and
HTML layouts are outside the supported Markdown subset.

The publisher turns relative image references into immutable repository URLs in
the GitHub release body. It never changes the original document for packaging.
Rendering belongs to `src/patch_notes/`; the application supplies navigation,
fonts, HTTPS transport and image textures through its public interfaces.
