#include <string>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <tuple>
#include <elf.h>
#include <unistd.h>
#include <cstdlib>
#include "elfhlp.hpp"
#include "demov.hpp"

int main(int argc, char** argv) {
	std::string file;
	std::string out_file, g_file, i_file;
	bool out = false;
	bool graph = false;
	bool idc = false;
	bool patch_call = false;
	int opt;
	elfhlp elf;
	uint64_t entry;

	while ((opt = getopt(argc, argv, "o:g:i:c:h")) != -1 ) {
		switch (opt) {
			case 'o':
				out = true;
				out_file = std::string(optarg);
				break;
			case 'g':
				graph = true;
				g_file = std::string(optarg);
				break;
			case 'i':
				idc = true;
				i_file = std::string(optarg);
				break;
			case 'c':
				patch_call = true;
				break;
			case 'h':
				std::cout << "The demovfuscator supports the ";
				std::cout << "following parameters:" << std::endl;
				std::cout << std::endl << " ./demov [-h] [-i ";
				std::cout << "symbols.idc] [-o patched_bin] [-g ";
				std::cout << "cfg.dot] obfuscated_input" << std::endl;
				std::cout << std::endl << " -h Use for a description ";
				std::cout << " of the options" << std::endl;
				std::cout << " -i Derive symbols from the input bin ";
				std::cout << "and store them into symbols.idc" << std::endl;
				std::cout << " -o Generate a UNIX dot compatible ";
				std::cout << "file containing the control flow" << std::endl;
				std::cout << "	graph (might be easier to read than ";
				std::cout << " IDA\'s graph view)" << std::endl;
				std::cout << "	Convert the .dot file to something ";
				std::cout << "usable by" << std::endl << std::endl;
				std::cout << "	cat cfg.dot | dot -Tpng > cfg.png";
				std::cout << std::endl;
				exit(0);
			default:
				std::cerr << "Usage: demov [-h] [-o patched] ";
                std::cerr << "[-g graph] [-i idc]"<< std::endl;
				exit(1);
		}
	}

	for (int i = optind; i < argc; i++) {
		int tmp;

		file = std::string(argv[i]);
		if (0 > elf.open(file)) {
			std::cerr << "cannot open file " << file << std::endl;
			continue;
		}
		demov de;
		std::unordered_map<uint64_t, std::string> *rel;
		std::map<uint64_t, std::tuple<uint8_t *,uint64_t, int>> *seg;
		std::cout << (elf.isx86mov() ? "possibly" : "not");
		std::cout << " movfuscated" << std::endl;
		rel = elf.getrelocations();
		if (rel) {
			std::cout << "Relocations:" << std::hex << std::endl;
			for (auto const &i: *rel) {
				std::cout << i.second << " at " << i.first << std::endl;
			}
		}

		seg = elf.getsegments();
		if (seg) {
			std::cout << "Segments:" << std::endl;
			for (auto const &i : *seg) {
				std::cout << i.first << " - ";
				std::cout << i.first + std::get<1>(i.second) << " : ";
				std::cout << (((std::get<2>(i.second)) & PF_R) ? "R" : " ");
				std::cout << (((std::get<2>(i.second)) & PF_W) ? "W" : " ");
				std::cout << (((std::get<2>(i.second)) & PF_X) ? "X" : " ");
				std::cout << std::endl;
			}
		}

		entry = static_cast<uint64_t>(elf.get_entrypoint());
		std::cout << "The entry point is " << std::hex << entry << std::endl;
		if (0 > de.init()) {
			std::cerr << "error initialising demov" << std::endl;
		}

		de.set_segments(seg);
		de.set_relocations(rel);
		de.set_entrypoint(entry);

		if (seg)
			delete seg;

		std::cout << "parsing entry" << std::endl;
		if (0 > (tmp = de.parse_entry()))
			std::cerr << "error parsing entry: " << tmp << std::endl;
		de.scan();
		de.dump_stat();
		std::cout << "analysing binary" << std::endl;
		if (0 > (tmp = de.analyse()))
			std::cerr << "error analysing binary: " << tmp << std::endl;

		//std::cout << "parsing data" << std::endl;
		//if (0 > (tmp = de.parse_data()))
		//	std::cerr << "error parsing data: " << tmp << std::endl;
		std::cout << "Basic blocks:" << std::endl;
		std::string flow = de.dump_flow();

		auto bb = de.get_blocks();
		for (auto &x: bb) {
			de.resub(x.first, x.second - x.first);
		}

		std::cout << "getting rid of tables" << std::endl;
		std::cout << "Symbols:" << std::endl;
		std::cout << de.dump_syms();
		de.dump_regs();

		if (graph) {
			std::ofstream ofile(g_file.c_str(), std::ios::out);
			ofile << flow << std::endl;
			ofile << de.dump_calls();
			ofile.flush();
			ofile.close();
		}

		if (rel)
			delete rel;

		if (out) {
			std::ofstream ofile(out_file.c_str(), std::ios::out | std::ios::binary);
			ofile.write ((const char*) elf.get_buf(), elf.get_size());
			ofile.flush();
			ofile.close();
		}

		if (idc) {
			std::ofstream ofile(i_file.c_str(), std::ios::out);
			ofile << "#include <idc.idc>" << std::endl << std::endl;;
			ofile << "static main() {" << std::endl;
			ofile << de.dump_idc();
			ofile << "}" << std::endl;
			ofile.flush();
			ofile.close();
		}
	}
}
