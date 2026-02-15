# Language files

Language files provide translations for the text displayed in the application. They are stored as JSON and loaded at runtime.

## Purpose

- Allow Vertex to support multiple languages and reach a wider audience.
- Strengthen community efforts!

## Format

Language files are JSON documents. Each file typically maps message keys to translated strings. Keep the JSON valid and UTF-8 encoded.
<br><br>
**See existing files for examples of structure and key naming conventions.**

## Review & Safety

All translation submissions are reviewed before they are merged into the repository. Review ensures:

- Accurate wording (to a certain degree since languages can be nuanced).
- **No malicious or harmful content**

### Community Effort
- Please keep in mind that specifically the accurate wording part is best ensured through community effort.
- If your native language is not yet supported, feel free to translate Vertex to your language and submit a PR.
- If your native language is already supported, you can still contribute by improving existing translations or adding missing keys.

Submissions that fail manual review are rejected or asked for clarification.

## Potential issues

- Certain languages such as Arabic or Hebrew (which are written from right to left) could be displayed incorrectly, this hasn't been tested though.
- So to any RTL speakers, if you want to contribute translations for your language, please also test the rendering of the translated strings in the application and report any issues you encounter.

## Re-evaluation: storage and scaling

Language files work well today but grow over time. Considerations for future improvements:

- Store translations in a small database (SQLite) to allow incremental updates and easier tooling.
- Keep the on-disk JSON format for simplicity if contributor workflows require direct file edits.

Trade-offs:

- JSON files are simple and VCS-friendly.
- A database can enable richer tooling, search, and per-key metadata (context, notes, reviewer, versioning).

## Contributing

- Follow existing key names and structure.
- Run a JSON linter before submitting.
- Include context for ambiguous strings when opening a PR.
- Respect the review process—translations are reviewed before landing.

## Troubleshooting

- Large files: consider splitting by domain or feature to keep translations maintainable.
- If a translation introduces rendering issues, check for invalid escape sequences or malformed formatting placeholders.

## Notes

- Keep keys stable; renaming keys will require updates across code and translations.
- Prefer short, reuseable keys and include contextual comments in PR descriptions when necessary.

If you want, I can also produce a small tool or script to validate language files and check for missing keys across locales.
