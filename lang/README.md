# Adding a new translation

1. Copy `en.properties` to a new translation file.
2. Edit the file as needed.
  * If you're not translating a given key, skip it. `en.properties` will automatically be used to fill in the blanks.
  * Only lines starting with "#" are comments. An "#" in the middle of a line is not treated as a comment.
  * The "|" character is used as a line break. Note that it only works in some contexts (like dialog boxes) and not others (like menu entries).
  * Keep all Unicode diacritics, etc. - it's the script's role to adjust it for the limitations of the text display system.
3. Make a pull request. The developers will handle the rest.
