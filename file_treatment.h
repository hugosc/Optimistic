#include <vector>
#include <string>
#include <iostream>
#include <fstream>

std::vector<opt::transaction> file_input_treatment(std::string file){
	std::ifstream inFile(file, std::ios::in | std::ios::binary);
	char ch;
	std::vector<opt::transaction> transaction_list;
	std::string transaction_name;
	std::string object_name;

	if(!inFile ) {
	std::cout<< "Não foi possivel abrir o arquivo de entrada : "<< file<<"\n";
	}else{
		while (inFile.get(ch)){
			std::vector<opt::action> actions_sequence;
			std::vector<std::string> read_objects, write_objects;
			transaction_name = "";

			while(ch != ':'){
				transaction_name += ch;			
				inFile.get(ch);
			}
			while(ch != '\n'){
				object_name = "";
				if (ch == 'r'){
					inFile.get(ch);
					while(ch !=')'){
						if (ch != '(')
							object_name += ch;
						inFile.get(ch);
					}
					read_objects.push_back(object_name);
					actions_sequence.push_back(opt::action(opt::READ, object_name));
				}
				if(ch == 'w'){
					inFile.get(ch);
					while(ch !=')'){
						if (ch != '(')
							object_name += ch;
						inFile.get(ch);
					}
					write_objects.push_back(object_name);
					actions_sequence.push_back(opt::action(opt::WRITE, object_name));
				}
				inFile.get(ch);
			}
			transaction_list.push_back(opt::transaction(actions_sequence,read_objects,write_objects,transaction_name));
		}
	}
	return transaction_list;
}
