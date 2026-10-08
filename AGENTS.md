# Working conventions

- Keep the code readable: descriptive names, small functions where useful, named
  native addresses/field offsets, and comments explaining reverse-engineered
  assumptions and why checks are needed.
- Commit completed, coherent changes as work progresses. Run checks appropriate
  to the change before committing, and use descriptive commit messages.
- Keep generated build outputs and release archives out of Git. Preserve older
  release archives locally rather than replacing them.
- Implement behavior in readable C++ with Exlaunch hooks. When revising an
  existing binary-patch feature, replace its instruction payloads with hooks.
  Original instruction fingerprints for compatibility checks remain useful.
