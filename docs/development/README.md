# Development Docs

`docs/development/` は、WSL / VSCode / AI 支援を含む開発運用の入口です。

## Files

- `workspace-setup.md`: WSL2 + VSCode での作業ディレクトリ構成と初期セットアップ
- `ai-workflow.md`: Codex / Claude Code を前提にした AI 駆動開発の進め方
- `prompt-templates.md`: AI へ依頼するときのテンプレート集

## Scope

- build / test / logging の日常導線
- AI へ渡すべき最小コンテキスト
- 人間がレビューすべき境界

詳細な設計判断そのものは `foundation/`、`subsystems/`、`adr/` を参照する。
