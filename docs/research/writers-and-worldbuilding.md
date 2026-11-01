<!-- markdownlint-disable -->

# Tools for writers, worldbuilders and game narrative

Research snapshot from 2026-10-14, gathered to plan Jot. Prices and versions drift; check sources before relying on them.

Jot: tools and workflows for writers and worldbuilders (research as of 2026-10-14)

Scope note: this is web research only, no code changes. Some details come from vendor pages or third-party summaries, and those are flagged. Claims without a source are from general knowledge.

1. Novelists and long-form writers

Scrivener is the reference model. Its binder is a tree of small documents, and the same set can be shown as a corkboard (synopsis cards) or an outliner (columns of metadata). It has collections (saved or smart selections), snapshots (per-document versions you can compare), and a Compile system that maps "Section Types" to "Section Layouts" to produce DOCX, ePub or PDF (https://www.oreilly.com/library/view/take-control-of/9781492065791/). Other features: Mac-only Linguistic Focus, which dims everything except dialogue or one part of speech (https://kirkville.com/analyze-your-writing-with-scrivener-3s-linguistic-focus-tool/), project and session targets, labels and status, custom metadata, a research folder, and a split editor. There is no native Linux build. A .scriv project is a folder: a project.scrivx XML binder plus UUID-named folders holding content.rtf, notes and synopsis files. No official spec exists, so check the layout against real projects (https://forum.literatureandlatte.com/t/how-a-scrivener-project-is-structured/9717, https://forum.obsidian.md/t/potential-steps-to-import-a-scrivener-project-without-scrivener/102984).

Ulysses uses nested groups, filters (smart folders by keyword, text or date), and color keywords for tracking POV or arcs. It sets word goals with deadlines per sheet or group. "Material sheets" are excluded from counts and export. Export styles use its own ulss style language (https://ulysses.app/releases/, https://www.macstories.net/?p=50409).

Plottr's core is a timeline grid: chapters on one axis and plotlines on the other, with drag and drop. Its 2.0 "Series Bible" works across books. Users still ask for wiki-style linking and family trees (https://plottr.com/introducing-plottr-2/, https://plottr.com/series-bible-hacks/, https://www.capterra.com/p/264561/Plottr/reviews/?page=3).

Campfire has about 17-18 modules: Characters, Relationships (charts and family trees), Timeline with custom calendars, Encyclopedia, Maps (pins, nested maps), Magic, Species, Cultures and others. Each module is optional and priced separately (https://selfpublishing.com/?p=4969, https://www.campfirewriting.com/pricing).

Dabble offers a drag-and-drop plot grid, goals and cloud sync. Atticus is mainly a book formatter (print and ePub) and runs on Linux. LivingWriter is "Google Docs plus Scrivener" with structure templates. Wavemaker is a free offline PWA with cards, planning boards and the Snowflake method (https://www.dabblewriter.com/articles/dabble-versus-atticus-tools-for-authors, https://kindlepreneur.com/living-writer-review, https://scribecount.com/author-resource/writing-tools-for-authors/wavemaker-writing-app).

Novelcrafter's Codex is the most relevant model for Jot:

- Entries have aliases, matched case-insensitively with automatic English plurals.
- Mentions are tracked across the manuscript, summaries, other entries and chats, with a heatmap. Tracking can be turned off per entry.
- "Progressions" are scene-anchored additions or replacements to an entry, for example a scar gained in chapter 12. The AI only sees them from that scene onward.
  Sources: https://www.novelcrafter.com/help/docs/codex/aliases, https://www.novelcrafter.com/help/docs/codex/codex-mentions, https://www.novelcrafter.com/help/docs/codex/codex-tracking.

Aeon Timeline is a dedicated chronology tool. Events are linked to entities (characters, places), and it keeps narrative order and chronological order separate. Its Scrivener sync maps documents to events and writes entity links into a metadata field (https://www.aeontimeline.com/guides/sync-with-scrivener). Keeping "narrative order vs story-time order" as two separate orderings is a key concept for Jot.

Open-source references:

- novelWriter (Python, Qt6/PyQt6, GPL-3, latest 2.7.x on PyPI) is the closest relative to Jot (https://github.com/vkbo/novelWriter, https://pypi.org/project/novelWriter/).
  - Format: each document is a plain-text file with a metadata header. The tree lives in one nwProject.nwx XML file, other metadata is JSON, and the whole project is friendly to version control and sync tools.
  - Syntax: Markdown-like, plus "%" comments, "%synopsis:", and a keyword reference system. A note declares "@tag: Soren". Scenes then reference it with "@pov: Soren", "@char:", "@location:", "@plot:", "@time:", "@object:" or "@entity:". Each keyword is tied to a root-folder class (Characters, Locations and so on). The outline view shows which characters appear in each scene. Tags are case-insensitive since 2.2 (https://github.com/vkbo/novelWriter/blob/2bf7e8506cd855388498fdaa778bed877e566701/docs/source/write_projects.rst, https://orenraynes.bearblog.dev/novelwriter-tricks/).
  - Exports: ODT and HTML, with a manuscript build tool.
  - Lesson for Jot: explicit typed references are cheap and reliable, but users must add the tags by hand.
- Manuskript (Python 3, PyQt5, GPL) has premise/Snowflake planning, characters, plots, world, an outline and index cards, a storyline view, item tracking, a frequency analyzer, and Pandoc-based export. It saves as plain text. The latest tag is 0.17.0 (30 June, year not shown on the page, likely 2024). There are 578 open issues and it is still on PyQt5, so maintenance is slow (https://github.com/olivierkes/manuskript, https://github.com/olivierkes/manuskript/releases).
- bibisco (GPL-3, Electron) offers guided character "interviews" covering physical, psychological, sociological and attitude questions. It has a relations board, shows which characters and locations appear in each scene, and gives per-chapter statistics. Reviewers find it heavy-handed (https://self-publishingschool.com/bibisco-review/).
- STARC / Story Architect is C++/Qt, GPL (some sources say open-core), and in beta. It covers screenplays, novels, comics and stage, with mind maps and character/location statistics, and imports FDX, Fountain and Trelby. It is a direct Qt peer worth studying (https://github.com/story-apps/starc).

2. Screenwriting

- Final Draft 13: Beat Board (freeform canvas), Outline Editor (lanes), Navigator, ScriptNotes, track changes, colored revision pages, structure lines, typewriter mode, writing stats, and real-time collaboration (https://www.finaldraft.com/learn/final-draft-quick-start/).
- Highland Pro (March 2025; Mac and iOS only): Fountain-native, FDX/PDF import and export, a gender-representation analysis, an Overview structure view, The Shelf for snippets, and a title page builder (https://blog.quoteunquoteapps.com/highland-2-highland-pro-what-changed-what-stayed-the-same-and-where-to-get-it/, https://apps.apple.com/us/app/highland-pro/id6612007609).
- Fade In: paid, and listed as running on Linux (https://alternativeto.net/software/fadein). WriterDuet: web, real-time collaboration, Fountain and FDX. Trelby: open source, little activity. Beat: GPL, macOS, Fountain-native, with autocomplete for scene headings and characters, live scene numbers, index cards and timeline, revision generations and JS plugins (https://nofilmschool.com/beat-screenwriting-software).
- Fountain rules: a scene heading is a line starting with INT/EXT followed by a blank line. Character cues are in caps, followed by dialogue. Transitions end in "TO:". Forced elements use a leading "." (scene heading), "!" (action, added in 1.1), "@" (character), ">" (transition) and so on. A title page uses key: value lines. Notes are [[]], boneyard is /\* \*/, sections are "#" and synopses are "=" (https://fountain.io/?p=8, https://johnaugust.com/?p=8070).
- Fountain tooling: afterwriting (Node CLI, PDF with a JSON config) (https://cdn.jsdelivr.net/npm/afterwriting@1.17.3/docs/clients.md), screenplain (Python, to HTML/PDF/FDX), and Emacs fountain-mode (https://github.com/rnkn/fountain-mode/wiki/Absolute-Beginner's-Guide-(OS-X)).
- Fountain and Markdown clash: "#" means a section in Fountain but a heading in Markdown. Treat .fountain as its own document mode rather than a Markdown dialect.

3. Worldbuilding, TTRPG and game narrative

- World Anvil: 25+ prompt-driven article templates, linked timelines, family trees and an atlas (https://www.worldanvil.com/compare/world-anvil-vs-kanka).
- LegendKeeper: auto-linking wiki, nested maps, whiteboards and templates; its timelines were still on the roadmap (https://www.legendkeeper.com/world-anvil-alternative).
- Kanka: everything is an "entity" (character, location, organization, race, quest and so on) with attributes and an is_template flag. Connections and relations are explicit and typed. @mentions in text are tracked separately from relations (https://kanka.io/en/api-docs/1.0/entities, https://docs.kanka.io/en/latest/_sources/features/connections.md.txt).
- Obsidian as a worldbuilding stack: YAML properties plus templates. Dataview or the core Bases plugin give table, list, card and map views over front matter, stored in .base YAML files (https://obsidian.md/help/bases). Fantasy Statblocks provides bestiaries (https://www.obsidianstats.com/plugins/obsidian-5e-statblocks). Leaflet gives image or tile maps with markers linked to notes, defined in YAML code blocks or front matter, and is now in maintenance mode (https://community.obsidian.md/plugins/obsidian-leaflet-plugin). World Builder and Universe Builder add entity kits. This is the strongest evidence that "markdown + front matter + typed views" works for this audience.
- articy:draft X: template-typed entities (objects inherit their template's properties) and a flow graph of dialogue and fragments. It has Unity and Unreal importers and JSON/XML technical exports that can be filtered by template (https://www.articy.com/en/articydraft/integration/techexports/).
- Arcweave: boards, elements and "components" (entities with attributes attached to flow nodes), variables and conditions. It exports JSON/CSV/XLSX and has a Web API and plugins for Unity, Godot and Unreal (https://docs.arcweave.com/integrations/json).
- Ink (inkle): plain-text knots (===), stitches (=), diverts (->), choices (\* once, + sticky). It compiles with inklecate to a documented JSON runtime format. Inky is a two-pane editor with live preview (https://github.com/inkle/ink/blob/b1d54e7444713a84770c2df1dbc6fdc83706d7aa/Documentation/ink_JSON_runtime_format.md).
- Yarn Spinner: .yarn plain text. Each node has headers ("title:"), then "---", the body, and "===" (https://docs.yarnspinner.dev/3.1/write-yarn-scripts/scripting-fundamentals/lines-nodes-and-options). v3 (May 2025) added enums, smart variables, once-only content and storylets with saliency (https://docs.yarnspinner.dev/3.1/changelog).
- Twine/Twee 3: the IFTF spec for plain-text Twine. Passages are ":: Name [tags] {metadata}" with a JSON StoryData passage, and Tweego compiles them (https://blog.iftechfoundation.org/2020-03-04-three-twine-specs.html).
- GDD practice: a living, modular, wiki-like document. Keep it lean (vision, pillars, core loop, audience, key features), give each section an owner, and keep changelogs and decision rationale (https://en.wikipedia.org/wiki/Game_design_document, https://www.wayline.io/blog/lean-gdd-game-design-documentation).

4. Character exploration: recurring needs

- Typed profiles, with optional fields so the form does not overwhelm (Campfire's lesson).
- Guided questionnaires and interviews (bibisco).
- Aliases and nicknames (Novelcrafter).
- Mentions and backlinks, with appearance heatmaps per chapter (Novelcrafter, bibisco, STARC).
- POV versus present-in-scene distinction (novelWriter @pov vs @char).
- Typed relationships with graph and family-tree views (Campfire, Kanka, bibisco). Plottr users explicitly ask for this.
- State that changes over time (Novelcrafter progressions, Aeon entity-event links).
- Images and moodboards, voice and dialogue samples.
- Name generators: low value, many free alternatives exist.
- Consistency checks (eye color, ages against the timeline). Only structured fields make these possible.
- Dialogue share and gender analysis (Highland, Final Draft): cheap to compute from Fountain cues.

5. Writer editor features

Expected in a writer's editor:

- Focus and typewriter scrolling, a distraction-free fullscreen mode.
- Session and project word targets with deadlines, history and streaks.
- Snapshots with diffs.
- Inline comments: novelWriter uses "%" lines, Fountain uses [[]], CriticMarkup is a Markdown option.
- Outline and corkboard over the same documents, with drag reorder.
- Split view and a research pane.
- Export through Pandoc: DOCX in Shunn manuscript format via a reference.docx or prosegrinder templates (https://www.autodidacts.io/convert-markdown-to-standard-manuscript-format-odts-docs-and-pdfs-with-pandoc/, https://pandoc-templates.org/template/prosegrinder-pandoc-templates/), ePub, and PDF through LaTeX or Typst.
- Fountain to screenplay PDF via afterwriting/screenplain, or a native layout engine.

6. Proposed feature set for Jot

Storage model, the foundation for every tier:

- A "vault" folder of .md files with YAML front matter. Optional project root file: jot.yaml (or .jot/project.yaml).
- The tree order goes in the project file, not in filename prefixes. This is novelWriter's approach and keeps renames cheap. A fallback can read "order:" from front matter.
- Types live in .jot/types/\*.yaml. Each type has a name, icon, color, a properties schema (string, number, date, enum, list, link-to-type, image) and a body template.
- A note declares "type: character". Unknown keys are kept untouched.
- A rebuildable index in SQLite (not source of truth, can be thrown away) holds links, mentions, properties and word counts.
- Everything stays git- and Syncthing-friendly.

Tier 1: core notes (must be excellent)

- Markdown editor with live styling, wikilinks plus backlinks, tags, full-text search, a quick switcher.
- Front matter shown as an editable property panel.
- Templates.
- Split view.
- Local history, using git underneath or a .jot/snapshots store.
  These are table stakes. If they are weak, nothing else matters.

Tier 2: writer mode (project = manuscript)

- A binder over an ordered folder. Front matter fields for scene/chapter type: status, label, synopsis, pov, characters, location, story_date.
- Outliner (table of those fields) and corkboard (synopsis cards), both with drag reorder writing back to the project file.
- Scrivenings-style "edit many as one" view.
- Targets (project, session, per-document, deadline), with a session-stats log in .jot/stats.jsonl and streaks.
- Focus, typewriter and fullscreen modes.
- Named snapshots with word-level diffs.
- Comments as CriticMarkup or a "%" convention.
- Exclude-from-compile flag (Ulysses material sheets).
- Compile = concatenate the ordered docs, then run Pandoc with bundled presets (Shunn DOCX, ePub, PDF), shelling out to a system pandoc.
- Fountain as a sibling document mode: highlighting, scene and character autocomplete, a scene navigator, and PDF through a bundled layout or afterwriting.

Tier 3: worldbuilding and character mode (the differentiator)

- Codex: any note whose type is flagged "entity" gets "aliases:" and auto-mention detection. Plain-text matches are underlined, but files are not rewritten. Optional "make explicit link" action.
- Mentions panel and per-chapter appearance heatmap. "Scenes this character is in" uses explicit pov/characters fields plus detected mentions, labeled separately.
- Typed relationships in front matter, e.g. relationships: [{to: "[[Mara]]", kind: sister}]. Rendered as a graph and family tree (QML Shapes or a force layout).
- Character type template with optional sections: interview questionnaire, voice samples, moodboard (image grid from a folder or list).
- Timeline: events are notes with a "date" (custom calendars later) and entity links. Two orderings: narrative versus story-time (Aeon).
- Progressions: dated or scene-anchored property overrides, e.g. "changes: [{at: '[[Ch12-scene3]]', eye: patch}]". Consistency checking compares scene-time state against the text and the timeline.
- Database views (table/cards/board) over properties, like Obsidian Bases, saved as .jot/views/\*.yaml.
- Image maps with pins linking to notes (Leaflet-like, using QML Image plus overlays). No tile servers.

Tier 4: game narrative

- Syntax highlighting, outline and link graph for .ink, .yarn and .twee files. Their knots, nodes and passages become first-class navigable items.
- Codex mentions work inside them, so character voice and appearance tracking extends to dialogue.
- "Compile and validate" by shelling out to inklecate, ysc or tweego and showing errors inline. Optional preview by running the compiled story (inkjs/yarn runtime in a WebEngine view, or C# not needed).
- GDD project template: vision, pillars, core loop, systems, entity types for items, quests and factions, a decision log.
- JSON export of entities and properties for engine import, covering most of what articy's technical export does for entities.

High value relative to cost: the type and property system with templates, the binder/outliner/corkboard over plain files, aliases plus mention tracking, targets and stats, snapshots with diff, Pandoc compile presets, Fountain mode, and the relationship graph from front matter. These all reuse one index.

Scope traps:

- Real-time collaboration and cloud sync: leave to git or Syncthing.
- WYSIWYG print layout and a native ePub/PDF typesetter: delegate to Pandoc.
- A full node-graph flow editor like articy/Arcweave: render the graph read-only from text instead.
- Custom calendar systems beyond simple eras.
- Map tiles and GIS.
- Built-in AI: optional plugin at most. Novelcrafter's progressions idea is valuable without AI.
- Name generators.
- Linguistic analysis beyond dialogue share and word frequency.
- Writing a full Fountain PDF paginator with industry-exact page breaks and (MORE)/(CONT'D): significant work, so reuse afterwriting first.
- Lossless round-trip Scrivener import: RTF to Markdown loses styles. Make it one-way import only.

Open formats to support:

- Read and write: CommonMark/GFM plus YAML front matter, Pandoc Markdown extensions (footnotes, citations, fenced divs), CriticMarkup, Fountain 1.1, Ink, Yarn (v2/v3), Twee 3.
- Import only: Scrivener .scriv (parse .scrivx, convert RTF through pandoc), novelWriter projects (plain text with "@" keywords, which map cleanly onto front matter), Obsidian vaults (natively compatible), FDX (XML) for screenplays.
- Export: Pandoc targets (DOCX, ODT, ePub, PDF, HTML), Fountain to PDF/FDX, and entity JSON.

Unverified items: the exact nwProject.nwx schema, the Manuskript release year, STARC's license (sources conflict), Fade In Linux support (third-party listing only), Kanka's relations explorer details, and current pricing for any product.
