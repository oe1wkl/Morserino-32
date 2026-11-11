# Snapshot bundles — a guide for authors

A **snapshot bundle** is one file that holds a set of practice presets — a CW
school's course, for example. A student opens it in the
[M32 Configuration Tool](https://www.morserino.info/m32_config_tool.html)
(Snapshots tab, **Import Snapshot Bundle…**), and the tool stores every preset in
its own snapshot slot on the Morserino in one go. Afterwards the student recalls a
preset on the device as usual: a short press of FN in the preferences, or the
Recall button in the tool.

This guide is for whoever writes such a file. You need a text editor and nothing
else. A complete, working example is the CW Schule Graz bundle:
[`Software/Utilities/snapshot-bundles/cw-schule-graz.json`](../../Software/Utilities/snapshot-bundles/cw-schule-graz.json).

## What happens when a student imports a bundle

1. The tool checks the whole file against the student's Morserino **before it
   changes anything**: every slot, menu, setting name and value. Problems are
   shown next to the preset they belong to; a preset with a problem cannot be
   imported, the others still can.
2. The student sees what each preset sets and which of their slots will be
   replaced, chooses a spacing (if the bundle offers more than one) and may leave
   out single presets.
3. For each preset the tool starts from the **student's own settings**, applies
   what the preset sets, chooses its menu entry and stores the snapshot. So
   everything a preset does *not* mention — speed, pitch, Koch lesson, colour
   theme, … — stays the student's own.
4. It reads every stored snapshot back and reports any setting that did not make
   it in (see "Settings a snapshot cannot hold").
5. It puts the student's own settings and menu position back.

## The file

A bundle is a [JSON](https://www.json.org) file. A small one:

```json
{
  "m32SnapshotBundle": 1,
  "name": "Our Club Course",
  "description": "Presets for the evening course.",
  "snapshots": [
    {
      "slot": 1,
      "name": "Random groups",
      "menu": "Koch Trainer/CW Generator/Random",
      "settings": {
        "InterWord Spc": 7,
        "Random Groups": "All Chars",
        "Length Rnd Gr": "5",
        "Max # of Words": 20
      }
    },
    {
      "slot": 2,
      "name": "Keyer",
      "menu": "CW Keyer"
    }
  ]
}
```

### Top level

| Field | | Meaning |
|---|---|---|
| `m32SnapshotBundle` | required | Always `1` (the format version). |
| `name` | recommended | Shown as the title of the import. |
| `description` | optional | One or two sentences for the student. |
| `spacing` | optional | Two or more spacing forms of the same presets — see below. |
| `snapshots` | required | The list of presets. |
| `note` | optional | Anything you want to remember; the tool ignores it. |

### Each preset

| Field | | Meaning |
|---|---|---|
| `slot` | required | The snapshot slot, **1 to 8**. Each slot only once per bundle. |
| `name` | recommended | What the student sees in the list. |
| `menu` | optional | The menu entry the snapshot starts in. |
| `settings` | optional | The settings this preset sets, and only those. |
| `note` | optional | Shown to the student, in italics, under the preset. |

A preset needs a `menu` or `settings` (or both).

### Menus

Write the menu entry as its **path**, the way the Morserino's menu is built,
with `/` between the levels:

- `CW Keyer`
- `CW Generator/Random`, `CW Generator/File Player`
- `Echo Trainer/Call Signs`, `Echo Trainer/Prefixes`
- `Koch Trainer/Select Lesson`
- `Koch Trainer/CW Generator/Random`
- `Koch Trainer/Echo Trainer/Adapt. Rand.`

Capital letters do not matter. Never use a menu *number*: the numbers differ
between the M32 Pocket and the 1st/2nd edition, and between firmware versions.
The names do not. A snapshot export from the Configuration Tool shows the path of
its menu as `menuName`, if you want to copy one.

### Settings

Use the setting's name exactly as the Morserino shows it in its preferences
(and as the Configuration Tool's Preferences tab lists it), for example
`InterWord Spc`, `Each Word 2x`, `Max # of Words`. Capital letters do not matter.

Write the **value the way the Morserino shows it**:

- a setting with a list of choices takes the choice's text, in quotes:
  `"Random Groups": "All Chars"`, `"Each Word 2x": "ON"`,
  `"Length Rnd Gr": "2 to 6"`, `"Length Calls": "3"`.
  For `Length Calls`, `"3"` (or just `3`) means call signs of up to 3
  characters — the choice that reads "3" — never "the fourth choice";
- a plain number stays a number: `"InterWord Spc": 45`, `"Max # of Words": 15`
  (0 means no limit). Some settings only take steps — **Max # of Words** goes in
  fives.

If a value is misspelt, the tool says so and lists the choices that exist.

Leave out every setting you do not need. A short preset is the normal case, and
it is better for the student: what you leave out stays their own.

### Two spacings of the same presets

A course often practises the same thing in two ways: with wide spacing for
writing down what you hear, and with no extra spacing (Koch spacing) for head
copy. A student has only eight slots, so they cannot keep both forms of each
preset. Instead the bundle offers the forms, and the student picks one when
importing:

```json
"spacing": {
  "wide": { "name": "Wide spacing: for writing down what you hear", "default": true },
  "koch": { "name": "Koch spacing: for head copy" }
},
"snapshots": [
  {
    "slot": 4,
    "name": "Koch groups of 2 to 6",
    "menu": "Koch Trainer/CW Generator/Random",
    "settings": { "InterWord Spc": 45, "Interchar Spc": 15, "Length Rnd Gr": "2 to 6" },
    "koch": { "InterWord Spc": 6, "Interchar Spc": 3 }
  }
]
```

`settings` holds the form marked `"default": true`. For each other form, a
preset may have an object named after it (here `"koch"`) with **only what
differs**; everything else comes from `settings`. A preset without such an
object is the same in every form. The names `slot`, `name`, `menu`, `settings`
and `note` cannot be used as form names.

## Settings a snapshot cannot hold

A snapshot keeps the settings that belong to a *kind of practice*. Settings about
the device itself are not stored, for example **Time-out**, **Serial Output**,
**Encoder Click**, **Quick Start**, **Paddle Polar.**, transmitter and
Bluetooth/USB settings, and the game settings. The user manual (section
**Snapshots**) lists them. If a bundle sets one of them, the tool tells the
student after the import that it was left out, so test your bundle once on a
Morserino and remove such lines.

The **Koch lesson** is not part of a snapshot either. That is on purpose: a
preset is a kind of practice that the student repeats at whatever lesson they
have reached.

## Testing a bundle

1. Check that the file is valid JSON — the Configuration Tool tells you if it is
   not, usually because of a missing comma or quote. Any JSON checker in a web
   browser works too.
2. Import it on a Morserino with the Configuration Tool. The preview shows every
   problem before anything is written; fix them and load the file again.
3. Look at the result list after the import, then recall a few slots on the
   device.

Test on a Morserino with the firmware your students use. A bundle that uses a
setting only newer firmware has shows a warning on older firmware, and that one
setting is skipped.

## Sharing a bundle

A bundle is just a file: put it on your course's web page, send it by email, or
ask for it to be added to this repository under
`Software/Utilities/snapshot-bundles/`.
