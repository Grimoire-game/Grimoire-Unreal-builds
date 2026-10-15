# Grimoire for Unreal Engine

Use Grimoire game data inside Unreal Engine **5.4+**. Sign in from the editor,
import a game into a native Data Asset, then use its objects, localized text and
dialogs from Blueprints or C++. Your game reads the asset offline.

This is a native C++ plugin with separate `GrimoireEditor` and `GrimoireRuntime`
modules. It uses the same Public API v1 contract as the supplied Unity plugin.

## Install

1. Close Unreal Editor.
2. Copy this directory to `<YourProject>/Plugins/Grimoire/`. The descriptor must
   be at `<YourProject>/Plugins/Grimoire/Grimoire.uplugin`.
3. For the source distribution, build your C++ project with your engine's
   supported C++ toolchain. A Blueprint-only project can add an empty C++ class
   first to create project build targets.
4. Enable **Grimoire** in **Edit > Plugins**, and restart if requested.
5. Open **Window > Grimoire Connect**.

The source has no third-party dependencies. Only engine modules are required.
Version **0.2.2** targets UE **5.4+**; its Windows editor and game builds are
validated against UE **5.7.4**. Version 0.1.0 was also built against UE 5.4.2. Other
engine versions and operating systems require building from source and testing
in that environment. Prebuilt editor binaries, when supplied, are for the exact
UE version named on their archive.

The **UE5.7.4-Win64-Editor** archive additionally includes the tested Windows
editor DLLs, so a matching editor can load it without compiling first. Building
or packaging a game still requires a supported C++ toolchain; this is not a
precompiled shipping-game distribution.

## Connect and import

1. Sign in with your Grimoire account. Complete the two-factor prompt if needed.
2. Select your **company / game**.
3. Click **Import / update game**.

The default endpoint is `https://api.usegrimoire.com`. An alternative HTTPS
endpoint can be entered before signing in. Passwords, bearer tokens and 2FA
challenges stay in memory; sign in again when restarting the editor. API URL,
game selection and preview locale are per-project user settings.

The importer pages through objects and dialogs, fetches their full documents,
follows external dialog links, and includes source strings, translations and
game variables (including their elements and authored values).
It saves `/Game/Grimoire/G_<game UUID without hyphens>/Database`. Re-imports
update that same asset so existing Actor and Blueprint references remain valid.

Downloads are staged in memory: a failed/cancelled download preserves the
previous asset. Save errors are reported and leave the complete new snapshot
dirty for **Save All**. Read requests retry rate limits and temporary gateway
errors up to three times; writes are not automatically retried. An import that
exceeds 10,000 objects, dialogs or game variables stops with an explicit error.
Variable reads use `GET /api/v1/variables` and require `variables:read` API access.

Reference the Database from an Actor, Widget or another cooked asset to include
it in a packaged game. The entire referenced snapshot is cooked, including all
its dialog dependencies and strings. Editor-only raw object documents, attached
tasks and account credentials are excluded from cooked runtime data.

## Objects in Blueprints

In Connect's **Objects** tab, search by name, code ID or template. Select an
object, select an Actor in the level, and click **Link selected Actor**.
Click an object's row again, or **Close details**, to collapse it. Fields are
grouped into collapsible sections with separate names, types and values.
Alternatively, add a **Grimoire Object Link** component manually and set its
Database and Object Key (UUID or `code_id`).

Drag the component into your Blueprint graph and call:

- **Get Number**, **Get Boolean**, **Get Vector**, or **Get Text** with a field
  UUID or label. Each returns a success flag. An optional index selects a value
  from a multi-value field.
- **Get Field** for the whole typed value list, references and field metadata.
- **Get Referenced Object** to resolve an imported related object offline.
- **Get Object** for the complete object with current runtime field values.
- **Get Variable Element** to resolve a field's reference to a synced game
  variable element, including its display name, description and range.

Field labels are matched case-insensitively. Duplicate labels are ambiguous;
use their field UUIDs. Object/string/dialog keys are case-sensitive. A missing
object, reference or mismatched getter returns false rather than contacting
the API or inventing a value.

```cpp
#include "GrimoireObjectLink.h"

if (const auto* Link = FindComponentByClass<UGrimoireObjectLink>())
{
    double Health = 0;
    if (Link->GetNumber(TEXT("health"), Health))
    {
        // Apply Health to your gameplay system.
    }

    FGrimoireObject Level;
    if (Link->GetReferencedObject(TEXT("levels"), Level, 0))
    {
        // Level.Fields contains its imported data.
    }
}
```

Add `GrimoireRuntime` to your game's module dependencies when using C++.

## Localized UI and world text

Use the **Strings** tab to search text and preview a supported game language.
Missing translations display **not translated** in the editor. Players fall
back to source text.

- **UMG:** add **Grimoire Text** to a Widget Blueprint and set its Database and
  String Key. It updates when the game language changes. `LanguageOverride`
  can override the shared language for one widget.
- **World text:** select a string and click **Link text on selected Actor**.
  The **Grimoire Text Link** component updates TextRender components on that
  Actor. Its `OnTextChanged` event supports other targets.
- **Blueprint gameplay:** use **Get Game Instance Subsystem > Grimoire
  Subsystem > Set Language**. Pass an empty language for source text.
- **Direct lookup:** `Database.GetText(StringIdOrKey, Language)`.

The Strings tab's language selector also refreshes matching linked Actors in
loaded editor levels. Editor preview language does not force a player's runtime
language. Use **Save translation to Grimoire** to publish an edited translation,
then re-import to refresh the offline asset.

## Dialogs

1. Import the game.
2. In **Dialogs**, click **Add Dialog Player to level**. This creates a
   **Grimoire Dialog Actor**, or selects the existing one for this database.
3. From your interaction Blueprint, call **Start Dialog** on that Actor with
   a dialog key shown in the list.

The Actor creates a basic UMG view with speaker, text, Continue and answer
buttons. Its **Manage Player Input** option switches the first local player to
Game and UI input while a dialog is visible, shows the mouse, and returns to
Game Only input at the end. Disable this option if your game manages input
modes, uses split-screen, or needs another owning player.

For a custom UI, disable **Create Default Widget** and bind the Player
component's **On Line**, **On Choices**, and **On Dialog Ended** events. Call
**Next** or **Choose(Index)**. You can also create **Grimoire Dialog Widget**
yourself, call **Set Player**, and add it to the viewport.

The runner supports line/question/context/condition/setter/jump/external-dialog
and end nodes, legacy section graphs, answer visibility, pick-once answers,
dialog-scoped variables, ranges, translations and speaker names. Voice URLs are
exposed on each line; playback/downloading is left to your audio system.

### Context notes and node data

Select the **DialogPlayer** component on your Grimoire Dialog Actor and enable
**Show Context Notes** (enabled by default). Non-empty context nodes display
their text without a speaker or voice, then wait for **Next**. Turn this off to
skip them automatically, as in the Unity runner's context-note option.

In a Blueprint **On Line** or **On Choices** event, break the **Grimoire Dialog
Line** structure. Its **Node** contains the authored name, type, source text,
string ID, speaker ID, voice URL, translations, links, options and conditions.
**Is Context**, **Dialog Id** and **Language** identify the displayed line.
Each visible choice exposes its **Option Id** and **String Id**; use its
**Index** with **Choose**. The node retains all authored options, including
currently hidden ones, so option IDs are safer than array positions for lookup.

Use the player's **Get Current Node**, or the Database's **Get Dialog Node** /
**Get Dialog Nodes**, to retrieve data elsewhere. Node lookup accepts its UUID
or identifier. **Json Fields** and **Raw Json** preserve custom node metadata;
setter, condition, visibility and action payloads are also exposed as JSON.
Node source text is authored data; the line's **Text** is the localized display
text, with `#object:objectId;fieldId#` placeholders resolved from runtime values.

### Missing dialog data

In **Grimoire Connect > Dialogs**, expand a dialog. Its requirements report
follows linked dialogs and checks object fields, speakers, game-variable
elements, dialog variables, strings and node links. It includes references in
hidden answers, conditions, setters and inline text. Each row shows where the
data is used and how to fix it:

- Missing objects, fields, text or linked dialogs: **Import / update game**.
- Missing game-variable elements: **Sync variables**.
- Missing node or local-variable definitions after re-import: repair the
  reference or definition in Grimoire, then import again.
- Imported object data without an Actor: it is available offline. Use **Link
  selected Actor to this object** only when you need its scene representation.

**Refresh requirements** updates scene-link status. Blueprints can use **Get
Dialog Requirements** and inspect **Missing**, **Used By** and **Message**.
The player stores **Last Requirements** and logs missing entries at start when
**Warn About Missing Data** is enabled. This is a static check of imported data;
custom external value resolvers may supply values that the report cannot see.

Use `SetNumber`, `SetBoolean`, `SetText` before `StartDialog` to seed variables;
`GetVariableJson` and `SetVariableJson` expose arbitrary values to C++ and
advanced Blueprint integrations. Values reset on completion unless **Keep
Variables Between Runs** is enabled. **Last End Reason** distinguishes normal
completion from missing targets, unmatched conditions and loop limits.

Object-field and type-element references read the imported database and shared
runtime values from **Grimoire Subsystem**. Dialog setters and gameplay use the
same values throughout a Game Instance, including between dialog runs. Object
Link getters also read changed field values. Override the Player component's Blueprint-native
**Read External Value** / **Write External Value** events to connect your save
game or gameplay state. These events take precedence over the built-in state. These
runtime changes do not write to the API or modify the database asset.

## Game variables in Blueprints

1. Open **Window > Grimoire Connect > Variables** and click **Sync variables**.
   A full **Import / update game** also syncs them. Expand a variable to see its
   element names, IDs, default values and numeric ranges.
2. In your Blueprint, add **Get Game Instance Subsystem** and select **Grimoire
   Subsystem**.
3. From that subsystem, call **Get Game Number**, **Get Game Boolean** or **Get
   Game Text**. Supply your Database, the variable's name or UUID, and the
   element's name or UUID. Check the returned Boolean before using the value.
4. Use the corresponding **Set Game Number / Boolean / Text** node to change
   it. Subsequent dialog conditions and setters use the same value. Bind **On
   Game Variable Changed** when your gameplay UI needs change notifications.

Variable and element names are matched case-insensitively; ambiguous names
return false. Use UUIDs to avoid ambiguity. When using a globally unique
element UUID, the variable argument may be empty. Elements without an authored
value resolve to their element UUID; retrieve **Get Variable Element** from
the Database for their name, description and other metadata.

**Get / Set Game Variable Json** support arbitrary JSON values. Numeric setters
clamp to the authored range by default. **Get / Set Object Field Json** let
gameplay share object-field changes with dialogs and Object Links.

Values are scoped by Grimoire game within the Game Instance and survive dialog
completion and map changes. Call **Reset Game Values(Database)** to restore
imported defaults for both variables and object fields. Store values in your
own SaveGame if they must survive restarting the game. Dialog-local variables
remain separate and follow **Keep Variables Between Runs**.

Variable syncing stages a complete replacement of imported defaults. A failed
page leaves the previous library intact. A variables-only sync preserves
objects, dialogs, strings and the last full-import timestamp. Existing runtime
overrides remain until you reset them or restart Play.

## Click a character to approach and talk

Add **Grimoire Click To Talk** to the **PlayerController Blueprint** used by
your top-down GameMode. Assign the Database, Target Object Key (the NPC's UUID
or Code ID), Dialog Key, and Interaction Distance in centimetres. Leave Handle
Mouse Clicks enabled for the standard top-down mouse controls. The character
placed in the level must have a matching **Grimoire Object Link** and collision
that blocks **Visibility**. The level needs a navigation mesh, just as normal
click-to-move does.

Clicking the linked NPC starts navigation toward it, then starts the dialog
when the controlled pawn is within range and has an unobstructed line of sight.
The controlled pawn is excluded even if it uses the same Object Link. Clicking
the ground cancels the approach. During the conversation, movement is stopped;
it resumes after the dialog ends. Unreachable targets time out without playing
the dialog remotely. Check **Last Interaction Status** while debugging.

The component reuses a matching **Grimoire Dialog Actor** in the level, or
creates one at runtime if none is present. For custom Blueprint input, disable
Handle Mouse Clicks and call **Request Talk To(Actor)** / **Cancel Approach**.
This helper is intended for a local, single-player top-down controller.

## Speaker portraits that follow Mood

Create a **Data Asset → Grimoire Portrait Library** and assign it to the level's
**Grimoire Dialog Actor → Portrait Library**. Add one Characters entry per speaker,
with its Object Key, Mood Field (default `Mood`), Mood Variable (default `Mood`),
Fallback image, and Moods map. Map element UUIDs or names to images. Each image
accepts a Texture and normalized UV Min/Max; use `(0,0)` to `(1,1)` for a whole
texture, or a cell rectangle for a sprite sheet. UV origin is the top left.

The default widget shows the current speaker's portrait, reads runtime object
field changes immediately, and falls back to the imported field. Empty or unknown
moods use Fallback. Context notes, unmapped speakers and finished dialogs hide
the portrait. The library holds texture references for cooking and stays separate
from the imported database, so importing again does not overwrite art mappings.

In Blueprints, get **Grimoire Subsystem**, then call **Set Object Mood** with the
Database, `npc_bofur`, and `Angry` (or a Mood element UUID). Check its Boolean
result: the object, field and element must be imported. Dialog setters and
**Set Object Field Json** use the same state and update portraits too. Bind
**On Object Field Changed** for other gameplay reactions. **Reset Game Values**
restores the authored mood and refreshes the portrait. These changes are local
runtime state; they do not publish changes to Grimoire.

Custom UMG widgets can call **Resolve Portrait** on the library using the dialog
line, Database and Grimoire Subsystem. Use its returned Brush with an Image and
hide the Image when the result is false. Re-evaluate on line/choice and object
field events, and hide the Image when the dialog ends.

The configured MyProject2 asset is `/Game/Grimoire/Portraits/DA_DialogPortraits`.
It maps Bofur's original Unity sheet in this order: Happy, Sad, Neutral, Confused,
Angry, Surprised, Afraid, Proud, Laughing; Neutral is the fallback. Bartel has no
portrait assigned because no Bartel artwork was supplied.

## Editing and engine synchronization

The Objects tab shows informational fields and editors for fields marked
`hints.game_engine_editable`, respecting `read_only`. Numbers, booleans, vectors,
text, URLs and lists use typed controls. Reference targets are edited in
Grimoire. **Queue for review** sends the original `base_value` along with the
edit to `POST /api/v1/engine-commits`, with `source: "unreal"`.

The **Scene** tab lists linked Actors in loaded levels:

- **Queue transform** reads the current remote instance list and replaces only
  this Actor's entry, retaining other engines and levels. The change goes to
  Grimoire's review queue.
- **Reset to Grimoire** restores the approved transform with Unreal Undo
  support. Position, rotation and scale toggles are on the Object Link.
- **Unlink** queues removal of this Actor's remote entry and clears its local
  object key. Use this before deleting a linked Actor. Deleting an Actor alone
  does not issue a remote write.

Save the level before syncing. Identity combines the level package and the
Actor's GUID, so Actor label changes do not create a new instance. Renaming or
moving a level changes its identity: unlink affected Actors before moving it.
Only loaded levels are considered. API writes are disabled during Play mode.

Positions use **Unreal centimeters and axes**. Rotation `x/y/z` stores **roll,
pitch, yaw in degrees**; scale is dimensionless. There is no implicit conversion
between Unity and Unreal coordinate systems. Engine entries keep distinct IDs.

Queued changes are not approved automatically. Multiple pending full-instance
replacements may conflict; review them in Grimoire, then re-import the approved
data. Cancelling a request cannot retract a write already accepted by the API.

## Tasks and creation

**Tasks** loads workflow tasks and game members, supports assignee filtering,
and provides the game's valid status actions. Object-attached tasks/notes are
also visible in the Objects tab.

**Create** creates a draft library object immediately after a confirmation
naming the game and template. The template picker uses templates seen in
imported objects; an explicit template UUID can also be entered. Blank objects
and optional Code IDs work as in the Unity plugin. Re-import after creation.
Creating a draft does not automatically link an Actor or publish it.

## Scope relative to Unity

The editor API workflow is native to Unreal, and snapshots replace Unity's
generated C# data for objects, references, translations and dialog playback.
Unity-generated export ZIPs cannot run as Unreal code. This version does not
import those ZIPs, execute their generated Flow/Logic runtime, or stream their
optional playthrough telemetry. It also uses explicit transform sync/unlink
instead of Unity's automatic deletion hooks, and imports a complete game
snapshot rather than individual dialog assets.

These distinctions do not require any server changes for the implemented API
workflow. A live account integration check is still required in your game;
the automated API tests use synthetic responses and perform no real writes.

## Validation and source layout

`GrimoireRuntime` contains reflected data types, the offline database, Actor
links, localized widgets and the dialog runner. `GrimoireEditor` contains the
HTTP client, staged importer, editor window and reviewable write operations.

The sibling `UnrealValidation/` directory is a disposable C++ host project.
Run its `Validate.ps1` script to build editor/game targets and run **Grimoire**
automation tests. It includes a validation-only compatibility header for this
machine's MSVC 14.51 with UE 5.4; the distributable plugin does not change engine
headers or require that header on supported toolchains.

The 15 automation tests cover the API envelope, authentication/2FA/refresh/cancellation, a
101-object paginated import, failed-refresh preservation, draft and engine
commit payloads, typed fields, references, localization fallback, legacy
dialogs, choices, conditions, setters and automatic-loop protection. They also
cover paginated variable syncing and failed-sync preservation, context display,
node/choice metadata, shared values across dialog players, runtime Object Link
reads, localized inline values, and missing requirements across linked cycles.
Interaction tests cover target identity, rejecting the controlled pawn, nearby
dialog startup, preventing restarts during a conversation, and movement unlock.

Native integration follows Epic's [plugin module](https://dev.epicgames.com/documentation/en-us/unreal-engine/plugins-in-unreal-engine)
and [Data Asset](https://dev.epicgames.com/documentation/en-us/unreal-engine/data-assets-in-unreal-engine) patterns.


### Custom dialog and choice designs (0.2.3)

Set the placed Grimoire Dialog actor's **Dialog Widget Class** to a Widget Blueprint derived from **Grimoire Dialog Widget**. Keep **Create Default Widget** enabled: the actor creates the selected class and manages player input.

In the widget's Class Defaults, configure the Grimoire Designer widget names and **Choice Widget Class**. The dialog requires a text block and a Vertical Box for choices. Optional bindings provide the speaker, Continue button/frame, and portrait image/frame. The plugin fills the text, hides Continue while choices are shown, and updates the portrait from the actor's Portrait Library.

Create the choice Widget Blueprint with parent **Grimoire Dialog Choice Widget**, a Button named `ChoiceButton`, and a Text Block named `ChoiceLabel`. The plugin creates one instance per available answer and connects its click to the corresponding option. Edit fonts, colors, padding, and button states in this Blueprint. Keep these two widget names unchanged.

MyProject2 uses `/Game/UI/WBP_Dialog` and `/Game/UI/WBP_DialogChoice`. Its choices appear below the dialog text inside a scrollable text column, leaving space for the portrait. Do not manually place a fixed number of answer buttons.
