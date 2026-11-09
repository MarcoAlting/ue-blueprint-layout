# Layout pass for ProgrammaticToolset.execute_tool_script.
# Run it in a separate tool call after the build pass, so the graph editor has drawn the nodes.
# Fill in the CONFIG section from the build pass. Sandbox imports allowed: json, math, re, copy, time, datetime.
import json
import math

BT = 'editor_toolset.toolsets.blueprint.BlueprintTools.'
LT = 'BlueprintLayoutToolset.BlueprintLayoutToolset.'

# ---------------------------------------------------------------- CONFIG
BLUEPRINT = '/Game/Path/BP_Name.BP_Name'
# key -> node refPath (the build pass should return these)
NODES = {
    # 'entry': '/Game/Path/BP_Name.BP_Name:Func.K2Node_FunctionEntry_0',
}
# Main exec chain, left to right.
CHAIN = []  # ['entry', 'clear', 'loop', ...]
# key -> (exec output pin, exec input pin). None where the node has no such pin in the chain.
EXEC_PINS = {}  # {'entry': ('then', None), 'loop': ('LoopBody', 'Exec'), ...}
# chain node -> [(getter key, getter output pin, node input pin)], top input first.
GETTERS = {}  # {'add': [('g_add_box', 'Box_Rows', 'self'), ('g_add_row', 'Row', 'Content')]}
# Reroutes that sit on the exec row right after a chain node:
# chain node -> (knot key, source node key, source output pin). The next chain node moves right.
KNOTS_AFTER = {}  # {'set_vm': ('k_ix', 'loop', 'Array Index')}
# Branch rows: (source node key, source exec output pin, knot key, first branch node key,
#               its exec input pin, main-line node key to sit under)
BRANCHES = []  # [('loop', 'Completed', 'k_c', 'redraw', 'execute', 'cw')]
# ---------------------------------------------------------------- END CONFIG

GAP = 64
KNOT_GAP = 32
BRANCH_DROP = 160
BRANCH_KNOT_X = 96
N = {k: {'refPath': v} for k, v in NODES.items()}


def bt(name, args):
    return execute_tool(BT + name, json.dumps(args))['returnValue']


def lt(name, args):
    return execute_tool(LT + name, json.dumps(args))['returnValue']


def snap(v):
    return int(math.ceil(v / 16.0) * 16)


def bounds():
    by_path = {b['node']['refPath']: b for b in lt('GetNodeBounds', {'nodes': list(N.values())})}
    return {k: by_path[ref['refPath']] for k, ref in N.items()}


def move(k, x, y):
    bt('set_node_position', {'node': N[k], 'pos': {'x': int(x), 'y': int(y)}})


def straighten(src, src_pin, mover, mover_pin):
    lt('StraightenConnection', {'sourceNode': N[src], 'sourcePin': src_pin,
                                'nodeToMove': N[mover], 'pinToAlign': mover_pin})


def overlaps(B):
    keys = list(B)
    hits = []
    for i, a in enumerate(keys):
        for b in keys[i + 1:]:
            A, C = B[a], B[b]
            tol = 1  # bounds are fractional; touching getters may share 1 unit
            if (A['x'] < C['x'] + C['width'] - tol and C['x'] < A['x'] + A['width'] - tol and
                    A['y'] < C['y'] + C['height'] - tol and C['y'] < A['y'] + A['height'] - tol):
                hits.append([a, b])
    return hits


def run():
    B = bounds()

    # 1. x positions from real widths
    X = {}
    prev_right = None
    for k in CHAIN:
        x = 0 if prev_right is None else snap(prev_right + GAP)
        if k in GETTERS:
            x = snap(x + max(B[g]['width'] for g, _, _ in GETTERS[k]) + GAP)
        X[k] = x
        prev_right = x + B[k]['width']
        if k in KNOTS_AFTER:
            knot = KNOTS_AFTER[k][0]
            X[knot] = snap(prev_right + KNOT_GAP)
            prev_right = X[knot] + B[knot]['width']
    for i, k in enumerate(CHAIN):
        move(k, X[k], 0 if i == 0 else B[k]['y'])

    # 2. exec line
    for a, b in zip(CHAIN, CHAIN[1:]):
        straighten(a, EXEC_PINS[a][0], b, EXEC_PINS[b][1])

    # 3. getters: right-aligned, top one straightened, the rest stacked touching
    B = bounds()
    for k, items in GETTERS.items():
        right = X[k] - GAP
        top = items[0][0]
        move(top, right - B[top]['width'], B[top]['y'])
        straighten(k, items[0][2], top, items[0][1])
        y = bounds()[top]['y'] + B[top]['height']
        for g, _, _ in items[1:]:
            move(g, right - B[g]['width'], math.ceil(y))  # sizes are fractional: round up so getters touch, not overlap
            y = math.ceil(y) + B[g]['height']

    # 4. reroutes on the exec row
    for knot, src, src_pin in KNOTS_AFTER.values():
        move(knot, X[knot], B[knot]['y'])
        straighten(src, src_pin, knot, 'InputPin')

    # 5. branch rows
    for src, src_pin, knot, first, first_pin, under in BRANCHES:
        B = bounds()
        branch_keys = {knot, first}
        main_bottom = max(B[k]['y'] + B[k]['height'] for k in N if k not in branch_keys)
        lane_y = snap(main_bottom + BRANCH_DROP)
        # The reroute stays on the lane: do not straighten it to the source pin.
        move(knot, snap(B[src]['x'] + B[src]['width'] + BRANCH_KNOT_X), lane_y)
        move(first, B[under]['x'], lane_y)
        straighten(knot, 'OutputPin', first, first_pin)

    # 6. compile and check
    bt('compile_blueprint', {'blueprint': {'refPath': BLUEPRINT}})
    B = bounds()
    return {'overlaps': overlaps(B),
            'nodes': {k: [round(B[k]['x']), round(B[k]['y']), round(B[k]['width']), round(B[k]['height'])] for k in N}}
