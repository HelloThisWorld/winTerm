# Layout model v2

Workspace schema version 2 and Docking Model version 1 use one binary split tree for persistence, docking plans, validation, preview geometry, commit, rollback, and history. A preview does not maintain a second layout algorithm.

## Nodes

### Pane

```json
{
  "type": "pane",
  "paneId": "pane-1"
}
```

A pane ID is stable and maps to exactly one pane descriptor and, while running, exactly one session owner.

### Split

```json
{
  "type": "split",
  "orientation": "vertical",
  "ratio": 0.5,
  "first": {},
  "second": {}
}
```

`vertical` places `first` on the left and `second` on the right. `horizontal` places `first` above `second`. Ratios are finite and remain between `0.05` and `0.95`.

### Empty slot

```json
{
  "type": "emptySlot",
  "slotId": "slot-1",
  "preferredProfileId": null
}
```

An empty slot has a globally unique stable ID and optional safe profile reference. It has no pane descriptor, session, process, connection, command, working directory, output, clipboard content, or child node.

## Edge transformations

Moving `Source` around `Target` produces:

| Zone | Orientation | First | Second |
| --- | --- | --- | --- |
| Left | vertical | Source | Target |
| Right | vertical | Target | Source |
| Top | horizontal | Source | Target |
| Bottom | horizontal | Target | Source |

The default split ratio is `0.5`.

## Corner transformations

Corner docking uses nested splits and one empty slot. It never creates an L-shaped terminal pane.

```text
Top left                    Top right
vertical                    vertical
├─ horizontal               ├─ Target
│  ├─ Source                └─ horizontal
│  └─ Empty                    ├─ Source
└─ Target                      └─ Empty

Bottom left                 Bottom right
vertical                    vertical
├─ horizontal               ├─ Target
│  ├─ Empty                 └─ horizontal
│  └─ Source                   ├─ Empty
└─ Target                      └─ Source
```

The corner column uses 35 percent of the available width and its two rows begin at 50/50. Geometry clamps the effective result to minimum pane dimensions under the target DPI.

## Transformation pipeline

`LayoutTransformer` clones immutable input, applies one deterministic mutation, normalizes the result, and validates the proposed tree. It supports:

- insertion around a root or identified pane;
- same-tree movement after source removal;
- source removal with sibling promotion;
- direct empty-slot replacement;
- tab-root subtree insertion without flattening internal splits.

The center, tab-strip, and new-window targets are ownership operations, not pane replacement. Their plan retains the source subtree as a unit.

## Normalization

- A valid two-child split remains a split.
- A split with one child is replaced by that child.
- A split with no child becomes a caller-supplied fallback empty slot or no root.
- Two adjacent empty slots collapse to the first stable slot.
- Compatible nested splits are not flattened in v0.5 because preserving ratios exactly is more important than reducing tree depth.

Normalization never repairs a duplicate pane ID, duplicate slot ID, cycle, orphan pane, or unsupported node. Validation rejects those cases.

## Validation

Validation reports node, pane, empty-slot, and maximum-depth counts and rejects:

- a missing root or child;
- a graph cycle;
- duplicate pane, slot, or cross-type stable IDs;
- missing pane or slot IDs;
- unsupported node types;
- split ratios outside the supported range;
- node metadata that belongs to a different node type;
- pane limits or maximum depth;
- panes without a live session when ownership validation is enabled.

The Workspace validator additionally requires every pane descriptor to appear exactly once in the layout and all identifiers to be globally unique.

## Schema migration

Migration from schema version 1 to 2:

1. creates a backup before a stored Workspace is migrated;
2. preserves every pane and split node without rewriting its meaning or ratio;
3. adds `dockingModelVersion: 1`;
4. changes only the envelope schema version;
5. does not add an empty slot;
6. remains deterministic and idempotent.

Unknown version-2 layout node types are rejected. Runtime drag state, overlay state, preview state, drag tokens, terminal output, and live session references are never persisted.
