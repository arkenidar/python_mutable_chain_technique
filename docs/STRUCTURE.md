# Structure & growth policy

How this repository is organized, and how it scales as the work grows.

## The two roles

| file | role | growth | read it for… |
|---|---|---|---|
| `docs/THESIS.md` | **the synthesis** — distilled, stable | slow (rewritten, not appended) | the idea, coherently |
| `docs/CONVERSATION.md` | **the discourse log** — raw, growing | fast (every exchange appends) | the how / why, in order |
| `docs/RECORD.md` | **the process record** — plan, issues, knowns/unknowns | slow | status & open questions |
| `docs/INDEX.md` | **the reading guide** — navigation | slow | where to start |

## The growth policy

1. **Distill, don't accumulate.** New ideas go into `CONVERSATION.md` (the log).
   Periodically they are *distilled* into `THESIS.md` (the synthesis). The thesis is
   re-written to stay coherent and short; it is the single source of truth for "the
   idea", while the log is the archive of "how we got here".
2. **Chunk the log at a threshold.** When `CONVERSATION.md` passes ~1000 lines, split
   it into `docs/discourse/NN-topic.md` with an index, so no single file balloons.
3. **One entry point.** `docs/INDEX.md` always maps *thesis → discourse → record → code
   → roadmap*, so a newcomer is never lost regardless of size.
4. **Cross-repo, explicit.** This is the *theory* repo; the *research* repo is the
   sibling `copyjumpvm/` (the machine, compilers, programs, roadmap). Linked both ways.
5. **Optional, later.** A GitHub Pages / mkdocs site (sidebar TOC + search) once the
   chunked structure exists.

## Principle

> Publish the *distillate* (thesis); archive the *discourse* (log). Readers read the
> thesis; historians read the log. Keep them separate, and both can grow forever.
