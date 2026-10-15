---
name: Pull Request
about: Default template for code changes, bug fixes, and feature additions
---

# Pull Request

## Description

<!--
Provide a clear, concise summary of the changes and their purpose.
Focus on the *what* and the *why* (not just the how).
Example: "Implements the §12.9 Buffer Low refill in the BBC Speak External player."
-->

## Related Issues / Documents

<!--
Link to any relevant issues, discussions, or specification sections.
- Fixes #XXXX
- Aligns with docs/technical/datasheet/XXXX.md §Y.Y
-->

## Testing

<!--
- [ ] All new code has corresponding tests, classified by directory (app/tests/unit/ or app/tests/functional/, see TESTING.md)
- [ ] Existing tests pass (CI green)
- [ ] Manual verification steps performed (describe if applicable)
-->

- **Unit tests added/modified:**
- **Functional tests added/modified:**
- **Manual checks run:** <!-- output-hashes.sh, corpus-check.sh, benchmark.sh, BBC hardware -->

## Checklist

- [ ] Code follows the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html) and AGENTS.md §4
- [ ] All source files include SPDX headers (AGENTS.md §4.2)
- [ ] Pre-commit hooks pass (clang-format, cpplint, clang-tidy, SPDX check); no `--no-verify`
- [ ] New public APIs are documented
- [ ] Specification references added where applicable (AGENTS.md §4.3.6)
- [ ] Documentation updated per AGENTS.md §9.3, and `mkdocs build --strict` passes if `docs/` changed
- [ ] Documentation updated if behaviour changed (AGENTS.md §9.3)
- [ ] Reference data in `app/golden/` unchanged, or the change is intended and explained below
- [ ] Export hash baseline unchanged, or the change is intended and explained below
- [ ] No secrets or credentials committed

## Performance Impact (if applicable)

<!--
Note any expected performance changes (>5 % time, >10 % memory), measured with benchmark.sh.
-->

## Breaking Changes

<!--
List any backwards-incompatible changes (project files, export layouts, CLI options) or migration steps required.
-->

## Additional Context

<!--
Screenshots, logs, or references to external specifications.
-->
