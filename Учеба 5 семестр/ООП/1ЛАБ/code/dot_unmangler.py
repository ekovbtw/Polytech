import re
import subprocess
import sys

NODE_PATTERN = re.compile(r'(Node0x[0-9a-f]+) \[shape=record,label="\{(.*)\}"\];')
EDGE_PATTERN = re.compile(r'(Node0x[0-9a-f]+) -> (Node0x[0-9a-f]+);')

REPLACEMENTS = [
    ("std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >",
     "std::string"),
    ("std::basic_ostream<char, std::char_traits<char> >", "std::ostream"),
]


def demangle(names):
    result = subprocess.run(["c++filt"], input="\n".join(names),
                            capture_output=True, text=True, check=True)
    return result.stdout.splitlines()


def main():
    if len(sys.argv) != 3:
        print("Usage: python3 dot_unmangler.py <input.dot> <output.dot>")
        sys.exit(1)

    with open(sys.argv[1]) as input_file:
        lines = input_file.readlines()

    node_ids = []
    mangled_names = []
    edges = []
    for line in lines:
        node_match = NODE_PATTERN.search(line)
        if node_match:
            node_ids.append(node_match.group(1))
            mangled_names.append(node_match.group(2))
            continue
        edge_match = EDGE_PATTERN.search(line)
        if edge_match:
            edges.append((edge_match.group(1), edge_match.group(2)))

    labels = {}
    for node_id, name in zip(node_ids, demangle(mangled_names)):
        for old, new in REPLACEMENTS:
            name = name.replace(old, new)
        if "MyString" in name:
            labels[node_id] = name

    with open(sys.argv[2], "w") as output_file:
        output_file.write('digraph "MyString call graph" {\n')
        output_file.write('    rankdir=LR;\n')
        output_file.write('    node [shape=box, fontname="monospace", fontsize=10];\n')
        for node_id, label in labels.items():
            escaped = label.replace("\\", "\\\\").replace('"', '\\"')
            output_file.write(f'    {node_id} [label="{escaped}"];\n')
        for caller, callee in sorted(set(edges)):
            if caller in labels and callee in labels:
                output_file.write(f'    {caller} -> {callee};\n')
        output_file.write('}\n')


if __name__ == "__main__":
    main()