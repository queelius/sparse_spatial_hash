# Sparse Spatial Hash - Technical Whitepaper

This directory contains the technical whitepaper for the `boost_sparse_spatial_hash` library.

## Contents

- `paper.tex` - Main LaTeX source document
- `references.bib` - Bibliography in BibTeX format
- `paper.pdf` - Compiled PDF (generated)

## Compilation

To compile the paper, you need a LaTeX distribution (TeX Live, MiKTeX, etc.) with the following packages:

- `amsmath`, `amssymb` - Mathematical typesetting
- `graphicx` - Figure support
- `booktabs` - Professional tables
- `hyperref` - Hyperlinks and cross-references
- `listings` - Code syntax highlighting
- `xcolor` - Color support
- `algorithm`, `algpseudocode` - Algorithm typesetting
- `caption`, `subcaption` - Enhanced captions

### Standard Compilation

```bash
cd paper
pdflatex paper.tex
pdflatex paper.tex  # Run twice for cross-references
```

### With Bibliography (Optional)

If you want to use BibTeX for citations:

```bash
pdflatex paper.tex
bibtex paper
pdflatex paper.tex
pdflatex paper.tex
```

### Using latexmk (Recommended)

The easiest way to compile with all dependencies:

```bash
latexmk -pdf paper.tex
```

## Paper Structure

The whitepaper follows a technical report format suitable for library documentation, conference presentations, or workshop submissions:

1. **Abstract** - Summary of problem, approach, and key results
2. **Introduction** - Motivation, existing approaches, our solution, contributions
3. **Design and Abstractions** - Core concepts, generic programming approach, API design
4. **Implementation Highlights** - Hash table design, performance optimizations, memory efficiency
5. **Performance Evaluation** - Benchmark methodology and results
6. **Applications and Use Cases** - Practical examples from different domains
7. **Related Work** - Comparison with existing approaches
8. **Conclusions and Future Work** - Summary and future directions

## Target Audience

- C++ library developers
- Game engine programmers
- Computational scientists (molecular dynamics, particle simulations)
- Academic researchers in spatial data structures
- Conference/workshop reviewers for Boost submission or CppCon presentation

## License

Copyright (C) 2025 DigiStar Contributors

Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
