# LaTeX source — "A Fast GPU Algorithm for Carving Simulation"

Overleaf-ready source, corresponding exactly (in content) to
`../autocam_1_IJAMT_rev3.docx`.

- **Main file:** `main.tex`
- **Figures:** `figures/` (PNG). `image1..image14` are Figs 1–8; `image16..image24`
  are the Appendix B render grid.
- **Compiler:** compiles as-is with **pdfLaTeX** (Unicode symbols are mapped via
  `newunicodechar`); also builds with LuaLaTeX/XeLaTeX (they use `unicode-math`).
- **Red entries** = the newly-suggested application references [10]–[15] and their
  in-text citations, flagged in red for review (all verified on Crossref, DOIs included).
  Remove the `\textcolor{red}{...}` wrappers once accepted.

Notes for finalising:
- Author block and affiliations are plain paragraphs after the title (as in the docx);
  move into a journal template's `\author`/`\affil` when porting to the IJAMT class.
- Section numbers are typed into the headings (auto-numbering is off) so the in-text
  "§x.y" cross-references match; switch to `\label`/`\ref` if preferred.
- The repository URL in "Code and data availability" still contains the project name;
  anonymise or replace with an archival DOI for submission.
