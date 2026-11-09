---
name: ue-blueprint-layout
description: Use when creating, rewriting or tidying an Unreal Engine Blueprint graph (function, event graph, widget Blueprint) through the unreal-mcp BlueprintTools, or when a Blueprint graph is unreadable - nodes on one row, crossing or diagonal wires, long wires from Function Entry, overlapping nodes.
---

# Readable Blueprint layout

## Overview

`write_graph_dsl` and `arrange_nodes` produce graphs that compile but cannot be read. Build the
graph in two tool calls:

1. **Build pass**: create nodes and wires with `create_node` / `connect_pins` at rough positions.
2. **Layout pass** (a later tool call): read the real node sizes with `GetNodeBounds`, set x
   positions from them, and set every y with `StraightenConnection`. That is the editor's own Q
   command, so pins line up exactly for every node type and zoom.

Never hard-code pin offsets or node widths.

## Requirement: BlueprintLayoutToolset plugin

The layout pass needs the MCP toolset `BlueprintLayoutToolset.BlueprintLayoutToolset`:
`StraightenConnection`, `GetNodeBounds`, `AlignNodes`, `AddCommentBox`, `SetNodePosition`,
`DeleteNodes`. Check `list_toolsets`. If it is missing, ask the user to install it from the
`ue-blueprint-layout` repository (`python install.py plugin <project folder>`), build with
every editor on that engine closed (Live Coding of any open project blocks the build), and
restart the editor. The graph must be open in a Blueprint
editor. The tools open it, but nodes get widgets only after the editor draws a frame. That is
why the layout pass is a separate tool call.

## Graph structure rules

1. **One exec line.** All main-chain nodes are straightened onto one exec line. A loop body
   continues on this line to the right of the loop node.
2. **Getters, not wires.** Read each function input with its own input getter
   (`Variables|Get<Param>`), next to the node that uses it. Wire only exec from Function Entry.
   Member variables and `self` get the same treatment.
3. **Local variables.** Promote a value to a local variable when two or more nodes use it, or
   when its source is more than one node away. Always do this for `ForEachLoop` Array Element
   and Array Index used later in the body: SET them at the start of the loop body, on the exec
   line. Name a local after what it holds (`Row`, `SlotVM`, `SlotIndex`), not after the pin.
   Any other promoted value: SET it on the exec line directly after the node that produces it,
   wired straight from that output, then read it with getters everywhere else.
   Add locals first with `add_variable` / `add_object_variable` and `graph=<function graph>`.
   The `object_class` path: a Blueprint class is `/Game/.../BP_Name.BP_Name_C` (also shown as
   the value of a Class pin); a C++ class is `/Script/<Module>.<ClassNameWithoutPrefix>`.
   A node whose Target is self by default (pin unconnected) stays unconnected: no Self getter.
4. **Simplify when behavior is identical.** `ForLoop 0..Length-1` + `Get (a copy)` becomes
   `ForEachLoop`. Report each simplification to the user.
5. **Branches off the exec line** (for example `ForEachLoop` Completed) go to a lower row,
   160 below the lowest node of the main row. One reroute on that row, 96 right of the source
   node's right edge, so the wire goes straight down or to the right, never back to the left.
   The branch's first node goes on that row under a main-line node (for example under Create
   Widget), straightened to the reroute.
6. **Reroutes only where a wire would cross a node.** Never put a reroute directly after a pin
   when the next reroute alone clears the obstacle. Two SETs fed from one loop: the second
   value wire passes under the first SET, so put a reroute 32 right of the first SET,
   straightened to the source pin, and the second SET at least 64 right of the reroute. The
   short wire from that reroute up into the second SET is not level. That is expected.
7. **Comment boxes** with `AddCommentBox(graph, nodes, text, color)` around each logical step,
   only when the user asks for them.

## Spacing

- 64 between nodes on the exec line, x rounded up to a multiple of 16.
- Getters sit in a column in front of their node: column width = widest getter, 64 gap on
  both sides.
- One input: the getter is straightened to its pin.
- Several inputs on one node: getters in pin order, right-aligned (x = node x - 64 - getter
  width) and stacked touching each other (next y = previous y + previous height). Only the top
  getter is straightened. The others sit below it, so their wires are slightly diagonal. That
  is intended.

## Build pass

Rebuild from scratch: delete every node except Function Entry, then create all nodes. That is
simpler than rewiring existing nodes. Run it as one `ProgrammaticToolset.execute_tool_script`
(call `get_execution_environment` once first) with these helpers, and return a dict of
key -> node refPath for the layout pass:

```python
def call(name, args):   # BlueprintTools; tools without output return {'returnValue': None}
    return execute_tool('editor_toolset.toolsets.blueprint.BlueprintTools.' + name, json.dumps(args))['returnValue']

def create(type_id, x, y):
    return call('create_node', {'graph': GRAPH, 'type_id': type_id, 'pos': {'x': x, 'y': y}})

def pin(node, name, out):   # connect_pins and set_pin_value take PinIDs, not names
    info = call('get_node_infos', {'nodes': [node]})[0]
    for p in (info['output_pins'] if out else info['input_pins']):
        if p['name'] == name:
            return p['pin_id']
    raise RuntimeError('pin %s not on %s' % (name, node['refPath']))

def link(a, a_pin, b, b_pin):
    call('connect_pins', {'output_pin': pin(a, a_pin, True), 'input_pin': pin(b, b_pin, False)})
```

`describe_toolset` on BlueprintTools returns about 72k characters and overflows. The tools
used here are `find_nodes`, `delete_node`, `create_node`, `get_node_infos`, `connect_pins`,
`set_pin_value`, `set_node_position`, `add_variable`, `add_object_variable`,
`find_node_types`, `compile_blueprint` and `read_graph_dsl`.

## Layout pass

Use `layout_pass.py`. Write a filled-in copy to a scratch file (replace the CONFIG block with
`NODES`, `CHAIN`, `EXEC_PINS`, `GETTERS`, `KNOTS_AFTER` and `BRANCHES` from the build pass),
then pass that file's text as the `script` of `execute_tool_script`. Order inside the pass:

1. `GetNodeBounds` for all nodes, then x positions for chain, getter columns and reroutes.
2. Straighten each chain node to its predecessor's exec output, left to right.
3. Getters: place right-aligned, straighten the top one, stack the rest.
4. Reroutes on the exec row: straighten to their source pin.
5. Branch rows: reroute and first node placed, then straightened.
6. Compile, then `GetNodeBounds` again and check that no two node rectangles overlap.

## Build-pass gotchas

| Problem | Fix |
|---|---|
| Reroute type id differs between editor sessions (`\|\|AddRerouteNode...` / `\|AddRerouteNode...`) | `find_node_types(filter="AddRerouteNode")[0]` at runtime |
| `connect_pins` fails from Create Widget | Set the `Class` pin before connecting outputs; the output type is generic until then |
| Type ids from `read_graph_dsl` fail | Use ids from `find_node_types` (`Panel\|AddChildtoVerticalBox`, lower-case "to") |
| `read_graph_dsl` shows a wire through a reroute as nesting, e.g. `(SetSlotIndex (ForEachLoop ...))` | Not a wiring error. Check with `get_node_infos` |
| Broad `find_node_types` filter floods the context | Keep the filter specific |
| `StraightenConnection` says "not drawn yet" | Call it in a later tool call than `create_node` |

| Node | type_id |
|---|---|
| Function input getter | `Variables\|Get<Param>` |
| Local getter / setter | `Variables\|Default\|Get<Name>` / `Set<Name>` |
| Member getter | `Variables\|<BlueprintName>\|Get<Name>` |
| Self | `Variables\|Getareferencetoself` (output pin `self`) |
| Reroute | look up, see above (pins `InputPin` / `OutputPin`) |
| For Each | `Utilities\|Array\|ForEachLoop` (pins `Exec`, `Array`, `LoopBody`, `Array Element`, `Array Index`, `Completed`) |

## Verify

1. `GetNodeBounds` after the pass: no overlapping rectangles, getters end 64 before their node.
2. Ask the user to look. Do not save the asset unless asked.
