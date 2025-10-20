# Documentation Guide

This document explains how to build, preview, and deploy the documentation for **Boost.Spatial - Sparse Spatial Hash**.

## Documentation Stack

- **Generator**: [MkDocs](https://www.mkdocs.org/)
- **Theme**: [Material for MkDocs](https://squidfunk.github.io/mkdocs-material/)
- **Deployment**: GitHub Pages
- **CI/CD**: GitHub Actions

## Quick Start

### Prerequisites

- Python 3.8 or later
- pip (Python package manager)

### Install Dependencies

```bash
# Install MkDocs and required plugins
pip install -r requirements.txt
```

### Local Preview

```bash
# Serve documentation locally with auto-reload
mkdocs serve

# Open browser to http://127.0.0.1:8000
```

The local server will automatically reload when you edit documentation files.

### Build Documentation

```bash
# Build static HTML site
mkdocs build

# Output in: site/
```

### Deploy to GitHub Pages

```bash
# Deploy to gh-pages branch
mkdocs gh-deploy
```

This will:
1. Build the documentation
2. Push to `gh-pages` branch
3. Make it available at: https://spinoza.github.io/sparse_spatial_hash/

## Project Structure

```
sparse_spatial_hash/
├── mkdocs.yml              # MkDocs configuration
├── requirements.txt        # Python dependencies
├── DOCS_README.md         # This file
│
├── docs/                   # Documentation source
│   ├── index.md           # Home page
│   ├── license.md         # License information
│   │
│   ├── getting-started/   # Getting started guides
│   │   ├── index.md
│   │   ├── installation.md
│   │   ├── quick-start.md
│   │   └── tutorial.md
│   │
│   ├── user-guide/        # User guides
│   │   ├── index.md
│   │   ├── core-concepts.md
│   │   ├── topologies.md
│   │   ├── queries.md
│   │   ├── incremental-updates.md
│   │   ├── advanced-usage.md
│   │   └── common-patterns.md
│   │
│   ├── api-reference/     # API documentation
│   │   ├── index.md
│   │   ├── grid-config.md
│   │   ├── sparse-spatial-hash.md
│   │   ├── position-accessor.md
│   │   ├── queries.md
│   │   ├── statistics.md
│   │   └── type-aliases.md
│   │
│   ├── performance/       # Performance guides
│   │   ├── index.md
│   │   ├── benchmarks.md
│   │   ├── optimizations.md
│   │   ├── best-practices.md
│   │   └── comparison.md
│   │
│   ├── development/       # Development guides
│   │   ├── index.md
│   │   ├── building.md
│   │   ├── testing.md
│   │   ├── contributing.md
│   │   └── design-decisions.md
│   │
│   ├── submission/        # Boost/CppCon submission
│   │   ├── index.md
│   │   ├── boost-submission.md
│   │   ├── cppcon-proposal.md
│   │   └── checklist.md
│   │
│   └── stylesheets/       # Custom CSS
│       └── extra.css
│
└── site/                  # Generated site (git-ignored)
```

## Configuration

### mkdocs.yml

Main configuration file:

- **Site metadata**: Name, description, URLs
- **Theme configuration**: Material theme settings
- **Navigation**: Page organization
- **Plugins**: Search, minification
- **Markdown extensions**: Syntax highlighting, admonitions, etc.

### requirements.txt

Python dependencies:

- `mkdocs>=1.5.3` - Core documentation generator
- `mkdocs-material>=9.5.3` - Material theme
- `mkdocs-minify-plugin>=0.7.1` - HTML/CSS/JS minification
- `pymdown-extensions>=10.7` - Markdown extensions

## Writing Documentation

### Markdown Files

Documentation is written in Markdown with extensions:

```markdown
# Page Title

## Section

Regular text with **bold** and *italic*.

### Code Blocks

```cpp
// C++ code with syntax highlighting
sparse_spatial_hash<Entity, 3> grid(cfg);
\```

### Admonitions

!!! tip "Pro Tip"
    This is a tip admonition.

!!! warning "Watch Out"
    This is a warning.

!!! note "Note"
    This is a note.

### Links

[Link text](other-page.md)
[External link](https://example.com)

### Images

![Alt text](images/diagram.png)
```

### Navigation

Edit `mkdocs.yml` to modify navigation:

```yaml
nav:
  - Home: index.md
  - Getting Started:
    - getting-started/index.md
    - Installation: getting-started/installation.md
    # ... more pages
```

## Material Theme Features

### Admonitions

```markdown
!!! note
    Information note

!!! tip
    Helpful tip

!!! warning
    Warning message

!!! danger
    Critical warning
```

### Code Annotations

```cpp
sparse_spatial_hash<Entity, 3> grid(cfg);  // (1)!
```

1. This is an annotation that appears on hover

### Tabs

```markdown
=== "C++"

    ```cpp
    code here
    \```

=== "Python"

    ```python
    code here
    \```
```

### Cards Grid

```markdown
<div class="grid cards" markdown>

-   :material-icon:{ .lg .middle } **Title**

    ---

    Description

    [Link](page.md){ .md-button }

</div>
```

## Automatic Deployment

### GitHub Actions

Documentation is automatically deployed when:

- Pushing to `main` branch
- Documentation files change
- `mkdocs.yml` changes

See `.github/workflows/docs.yml` for configuration.

### Manual Deployment

```bash
# Deploy from local machine
mkdocs gh-deploy
```

### Deployment Process

1. GitHub Actions runs on push to main
2. Installs Python dependencies
3. Builds documentation with `mkdocs build`
4. Deploys to `gh-pages` branch
5. GitHub Pages serves from `gh-pages` branch

## Customization

### Custom CSS

Edit `docs/stylesheets/extra.css` to add custom styles.

Referenced in `mkdocs.yml`:

```yaml
extra_css:
  - stylesheets/extra.css
```

### Custom Colors

Edit `mkdocs.yml` theme palette:

```yaml
theme:
  palette:
    primary: indigo
    accent: indigo
```

### Custom Fonts

Edit `mkdocs.yml` theme fonts:

```yaml
theme:
  font:
    text: Roboto
    code: Roboto Mono
```

## Troubleshooting

### "Module not found" Error

```bash
pip install --upgrade -r requirements.txt
```

### Port Already in Use

```bash
mkdocs serve -a 127.0.0.1:8001
```

### Build Fails

```bash
# Build with verbose output
mkdocs build --verbose

# Build without strict mode
mkdocs build --no-strict
```

### Deployment Fails

Check GitHub Actions logs:
1. Go to repository on GitHub
2. Click "Actions" tab
3. Click on failed workflow
4. Review error messages

## Best Practices

### Writing Documentation

1. **Clear and concise**: Use simple language
2. **Code examples**: Include working code snippets
3. **Cross-references**: Link to related pages
4. **Consistency**: Follow established patterns
5. **Test examples**: Ensure all code examples compile

### File Organization

1. **Logical structure**: Group related content
2. **Index pages**: Provide overview at each level
3. **Clear filenames**: Use descriptive names
4. **Shallow hierarchy**: Avoid deep nesting

### Maintenance

1. **Keep updated**: Update docs when code changes
2. **Test links**: Verify all links work
3. **Review builds**: Check GitHub Actions status
4. **User feedback**: Incorporate user suggestions

## Contributing to Documentation

### Workflow

1. Fork the repository
2. Create a branch: `git checkout -b improve-docs`
3. Edit documentation in `docs/`
4. Test locally: `mkdocs serve`
5. Commit changes: `git commit -am "Improve documentation"`
6. Push branch: `git push origin improve-docs`
7. Create Pull Request

### Guidelines

- Follow existing structure and style
- Include code examples where appropriate
- Use proper Markdown formatting
- Test all links and code snippets
- Preview changes locally before submitting

## Resources

### MkDocs

- [Official Documentation](https://www.mkdocs.org/)
- [User Guide](https://www.mkdocs.org/user-guide/)
- [Configuration](https://www.mkdocs.org/user-guide/configuration/)

### Material Theme

- [Documentation](https://squidfunk.github.io/mkdocs-material/)
- [Reference](https://squidfunk.github.io/mkdocs-material/reference/)
- [Setup](https://squidfunk.github.io/mkdocs-material/setup/)

### Markdown

- [Python-Markdown](https://python-markdown.github.io/)
- [PyMdown Extensions](https://facelessuser.github.io/pymdown-extensions/)

## Support

Questions about documentation?

- GitHub Issues: https://github.com/spinoza/sparse_spatial_hash/issues
- GitHub Discussions: https://github.com/spinoza/sparse_spatial_hash/discussions

---

**Documentation URL**: https://spinoza.github.io/sparse_spatial_hash/
**Repository**: https://github.com/spinoza/sparse_spatial_hash
**Built with**: MkDocs + Material for MkDocs
