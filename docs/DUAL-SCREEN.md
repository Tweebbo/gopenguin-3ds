# Native minigame screen roles

The November 5 adaptation follows the existing Bean Counters, Puffle Roundup,
Ice Fishing and Cart Surfer integrations. Their controllers, models, data and
resource files are preserved. The other games reuse their existing loaded banks;
no additional art bank or lower-quality texture export is introduced.

| Game | Upper screen | Touch screen |
| --- | --- | --- |
| Astro Barrier | The original monitor fitted vertically, with the real completion score | A larger ship lane; drag to aim and fire, or use the direction/fire pads |
| Jet Pack Adventure | Flight overview that keeps a descending pilot visible | A closer live view; hold the flight area to thrust and steer, or use the pads |
| Aqua Grabber | Wider underwater context | A closer scene; hold a location to steer and use Claw for the grab action |
| Puffle Rescue | Full level context | A closer view; tap a visible cardinal direction for a model step |
| DJ3K | The original animated deck | The original deck's switches, effects and keys; Zoom keys enlarges the piano |
| Dance Contest | The original performance | The original chart and receptor positions, matched to stylus and physical lanes |
| Catchin' Waves | Surfing context | Original ocean art with drag steering/rotation and a Jump action |
| Paint By Letters | Existing illustration beside readable live story text | Current word, larger letter keys and page navigation |
| PSA 3–11 | Briefing, objective, progress and actual puzzle feedback | Original scene with the existing step, code, order or fuse controls |

Stylus input uses the displayed view's inverse where scene coordinates matter.
Release outside the play area clears held actions. Existing physical controls,
logic rates, model rules, payout confirmation and resource budgets remain in
place. The optional `GameController.draw_top` callback draws screen-space
readouts after the upper scene; existing controllers leave it unset.

The mission buttons still represent the existing abstract task flow. They do
not establish recovery of the original point-and-click interactions. Expanded
native book painting, remaining tint exports and other original-content gaps
remain separate work. This adaptation does not claim complete game parity.

Evidence, before/after captures, host tests, native builds and package identity
are recorded under `../../review/dual-screen-20261105/`. Physical-console
acceptance remains separate from Azahar evidence. No installation or publication
is performed by these changes.
