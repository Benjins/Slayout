Slayout
========
A layout engine for making documents I guess

-------------------

TODO:
 * ~~Better text layout, line breaks~~
 * ~~Text justification~~
 * Full Justification
 * Better row algorithm
 * ~~Columns~~
 * Better hinting, avoid missing pixels
 * Align-y, also align-x centre, left, or right
 * Customizble anchors (right now just assumes upper-left)
 * Output to PDF
 * Lines
   - ~~Have lines~~
   - Line thickness
   - ~~Line colour~~
   - Line style
 * Bullet points
   - Will likely require Unicode support
   - Also probably a good way of internally composing more verbs from the core set?
 * Better tooling/UX
   - Determine when something is overconstrained
   - Determine what properties can't be solved
   - Offer a 'partial' solution when possible, esp. when done interactively
   - Add editor for source in slayout_viewer
   - Offer error messages/log in slayout_viewer
   - Clamp framerate in slayout_viewer so it doesn't melt the CPU