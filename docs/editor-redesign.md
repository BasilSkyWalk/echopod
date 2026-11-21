# A simpler theme editor: the plan

This is the plan for rebuilding the theme editor's workspace so that someone who has never used a design tool can make a theme they like. It changes the web page only. The player's firmware, the theme file and the live preview stay as they are.

A clickable mock-up of the new workspace goes with this plan. It is a sketch with no real player behind it, made to be tried and argued with before anything is built.

## Why

Walking the current editor as a first-time visitor, recolouring a theme is easy (about five minutes) and making something of your own is hard (hours, and it helps to know a design tool). The reasons, as seen on the page:

- It is organised around how the player is built: nine steps, each a long form.
- Colours are 16 named slots with codes; elsewhere a colour is chosen by picking one of those 16 names from a list.
- Drawing a screen means working in a side panel: 56 controls before anything is picked, 91 with one text part picked.
- Every drawn screen is drawn twice, sideways and upright.
- Starting points and ready-made parts are plain text buttons with no picture.
- Things that open don't look like they open (the Fonts cards, the Wallpaper rows).
- The preview stays on whatever screen you were on, even when the step is about another one.
- Words such as "pull request" and sizes in KB.

## Who it is for

Most people who open it will not be experienced, and the editor should not lean on a large stock of ready-made themes or layouts. So the empty screen itself has to be easy.

The five things it must make easy, in order of how many people will do them:

1. Change the colours of the whole theme, and see that it still reads well.
2. Put a picture behind the lists.
3. Rearrange Now Playing: move the cover, the title, the progress bar.
4. Make Home their own: a list, tiles, the cover, what's playing.
5. Go further when they want to: their own letters and icons, parts that move with the music.

## What other editors do

Notes from the makers' own help pages and a few reviews. Anything not confirmed on a page is left out.

| Editor | What it does that helps | What gets in the way |
|---|---|---|
| Samsung Watch Face Studio | Parts are named after what they show ("Digital clock", "Progress bar") and arrive working. Three-colour theme palettes. The always-on face is made automatically, then you adjust it. A panel beside the screen sets the time, battery and steps to try. | A full property panel, and typed expressions for anything beyond the basics. |
| Facer Creator | Ready-made, pre-wired "essential" parts. A time slider. The wiring stays visible behind an Advanced panel, so it can be learned later. | Newcomers couldn't find where expressions live. The dimmed face is built by hand, layer by layer. |
| Zepp Watch Face Maker | Parts chosen by what they show; no formulas. | Nothing appears until you upload pictures (ten digit images for a clock). Places typed as numbers. |
| Canva | The bar changes with what is picked and shows only what applies. Rare controls sit one and two levels down. Colour belongs to the whole design: a palette of the colours in use, and "replace everywhere". One click restyles everything by role. | Adapting a design to another size is automatic but, by one how-to, needs hand fixes afterwards. |
| Garmin Face It | About four decisions: picture, arrangement, colours, name. | Almost nothing else can be changed. |
| Apple's Lock Screen | Every change is made on the thing itself: tap the time to change its lettering. | Nothing can be moved. |
| Kustom (KLWP) | Very capable. | Typed formulas are the only way to show live information, with two-letter function names. Each colour is tied to the theme by hand, one property at a time. People give up and adapt other people's presets. |

Sources: [Watch Face Studio guide](https://developer.samsung.com/watch-face-studio/user-guide/create.html), [its always-on page](https://developer.samsung.com/watch-face-studio/user-guide/always-on.html), [Facer's essential elements](https://docs.facer.io/b-element-types/essential-elements), [Facer community on dim mode](https://community.facer.io/t/dim-mode-facer-creator/87241), [Zepp's guide](https://docs.zepp.com/docs/guides/tools/watchface/guides/create/), [Canva's editor](https://www.canva.com/help/glow-up/), [Canva colours](https://www.canva.com/help/element-color/), [Canva styles](https://www.canva.com/help/color-and-font-combinations/), [Garmin Face It](https://support.garmin.com/en-US/?faq=lZgGmgQQ018Wjj9O5lrUr8), [Apple's Lock Screen guide](https://support.apple.com/guide/iphone/create-a-custom-lock-screen-iph4d0e6c351/ios), [Kustom functions](https://docs.kustom.rocks/docs/reference/functions/), [a design critique of KWGT](https://ixd.prattsi.org/2021/09/design-critique-kwgt-app/).

What the easy ones share:

1. What is picked decides which controls show.
2. Common controls are on the bar; rare ones are one step away.
3. Parts are named after what they show and look right the moment they are added.
4. Live information is picked, never typed.
5. A small set of theme colours drives everything.
6. The second screen shape is made for you, then adjusted.
7. The information the screen shows can be changed right beside it, to see every state.
8. A blank start asks one question, then opens the editor.

The current editor already does 4 and 7, and half of 3. It does not do 1, 2, 5, 6 or 8.

One thing none of them has: their preview is an imitation, and Samsung's users report faces that work in the preview and not on the watch. Ours is the player's own code. That stays.

## The new workspace

One workspace replaces the nine steps.

- **The screen is in the middle, and big.** Home, Lists and Now Playing are tabs above it, with Sideways and Upright beside them. The try-it bar (play, how far in, which song) stays under it.
- **Add to this screen, on the left.** A shelf of parts with a small picture each: Song title, Artist, Album, Album cover, Progress bar, Time played and left, Play button, Big time, Menu as a list, Menu as tiles, My own words, Box, Circle. Click one and it lands on the screen, looking right, ready to move. A part only shows on the screens it can go on.
- **Change things on the screen.** Click anything and a small bar appears beside it with its main controls: colour (the theme's colours as dots, plus "its own"), size, lining up, Remove, and More. This works on Lists too: click the highlighted row, the title bar or the background to recolour it.
- **Look, on the right.** Three colours set the whole theme: Background, Text and Accent. The other colours are made from them, and the panel says whether text is easy to read, with a Fix. Below: a few colour sets, colours from a picture, lettering, corners, and a place to drop a wallpaper.
- **Draw once.** The upright screen is arranged from the sideways one. Move anything there and it becomes yours; "Back to automatic" undoes that.
- **More.** Exact places and sizes, moving with the music, showing a part only in some states, and repeating a part stay, behind More on the part. Every single list colour, drawing letters and icons, accent choices and kept themes stay, behind More options in Look.
- **Put on my player.** One button, three plain steps.

## Where today's steps go

| Today | In the new workspace |
|---|---|
| Start | The name sits in the top bar. Beginning from another theme and kept themes move to a Themes button. |
| Colours | Look: three colours. All sixteen stay under More options. |
| Lists & shapes | Click the row or title bar on the Lists tab. Corners is in Look; the rest is under More. |
| Home, Now Playing | The same tabs, always drawings you can change. "The player's own" becomes the layout each one starts with. |
| Wallpaper | A drop area in Look. |
| Fonts | Lettering in Look: the player's own or one you add. Drawing letters is under More options. |
| Icons | More options. Tile icons arrive with the tiles. |
| Save & share | Put on my player. Sharing with the gallery keeps its own plain-worded page. |

## What stays and what goes

Stays: the live preview, the theme file, undo, kept themes and versions, colours from a picture, the reading checks, snapping and guides, the letter and icon drawing tools, and everything a theme can do today.

Goes: the nine steps as the way in; choosing a colour by name from a list; the long parts form as the first thing you meet; drawing the upright screen from nothing; "The player's own / My own drawing" as a choice to make.

Starting points stay few: Plain, Tiles and Empty for Home; Plain, the four faces and Empty for Now Playing. Each gets a small picture.

## Building it

Stage 1 is built. The rest is still to do.

Each stage leaves the editor working and better than before.

| Stage | What it delivers | Size |
|---|---|---|
| 1. The workspace | The new layout: shelf, big screen, Look. Screens as tabs. Today's forms move behind More. The small faults found in the walk-through are fixed here (the stray "null", the tour, the cards that don't look like they open, the wording). | Medium |
| 2. Three colours | The whole theme from Background, Text and Accent, with any colour still changeable by itself. Colour sets. | Medium |
| 3. The bar on the screen | Click a thing, change it there. On drawn screens and on Lists. | Large |
| 4. The shelf | Parts with pictures that land looking right. Menu as a list and as tiles swap in place. | Medium |
| 5. Draw once | The upright screen made from the sideways one, with a way back. | Large |
| 6. First visit | One question to begin (start from this, or empty), then the workspace. A short tour that points at real things. | Small |
| 7. Try it with people | Three people who have never seen it each make a theme. Fix what stops them. | Small |

Stages 3 and 5 carry the most risk. The bar has to work on the player's own preview, where the page must know what is under the pointer. The upright arrangement has to be good enough that most people leave it alone.

## How we'll know it worked

With someone who has never seen it, and no help:

- They change the theme's colours and put it on the player in under five minutes.
- They move the cover and the title on Now Playing without opening More.
- They make a tiled Home.
- They never see a colour code or a size in KB unless they go looking.
- Asked afterwards, they can say what the left side, the right side and the bar are for.

## Open questions

1. Does 0.13.0 go out with today's editor, or wait for stage 1 or 2? The firmware is finished and tested either way.
2. Are three colours the right number? Samsung uses three. A fourth for the title bar is the likely request.
3. Should the editor work on a phone? The mock-up does, with the panels under the screen. Dragging small parts with a finger is the hard part.
4. Whether to offer a few arrangements to pick from (as Garmin and Apple do) on top of free placement. This plan leaves them out, to keep the stock of ready-made things small.
