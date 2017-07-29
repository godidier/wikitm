//#define BOOST_LOG_DYN_LINK 1

#include "wikitm.hpp"


namespace fs = boost::filesystem;


wikitm::wikitm(){
	m_pages_count = 0;

}

wikitm::wikitm( boost::gregorian::date date_start, 
		boost::gregorian::date_duration date_delta, int date_count, 
		std::string input_folder, std::string output_folder){

	m_dumpfiles = gen_dumplist(input_folder); 
	m_timeline = gen_timeline(date_start, date_delta, date_count);
	m_outfiles = gen_outfiles(m_timeline, output_folder);
	m_pages_count = 0;

}




std::vector<std::string> wikitm::find_pages(std::string& chunk){
	std::vector<std::string> pages;
	while(true){  
		bool has_page_start = false;
		bool has_page_end = false;
		int i_pstart = chunk.find( PAGE_TAG_START );
		int i_pend = chunk.find( PAGE_TAG_END );
		if ( i_pstart != std::string::npos ){
			has_page_start = true;
		}
		if ( i_pend != std::string::npos ){
			has_page_end = true;
			i_pend += std::string(PAGE_TAG_END).size();
		}

		if ( has_page_start && has_page_end ) {
			if ( i_pstart < i_pend ) {
				 pages.push_back( chunk.substr(i_pstart,i_pend) );
			} 
			else { 
				has_page_end = false;
			}
			chunk = chunk.substr(i_pend, std::string::npos );
		} 
		else if ( !has_page_start && has_page_end ) {
			chunk = chunk.substr(i_pend, std::string::npos );
		}
		else if ( has_page_start && !has_page_end ) {
			chunk = chunk.substr(i_pstart, std::string::npos);
			break;
		}
		else if ( !has_page_start && !has_page_end) {
			break;
		}
	}
	return pages;
}


std::vector<char*> wikitm::find_pages(char* chunk, char* trail)
{
	std::vector<char*> pages;
	while(true){
		bool has_page_start = false;
		bool has_page_end = false;
		char* pstart = NULL;
		char* pend = NULL; 
		unsigned long page_length = 0;

		pstart = std::strstr( chunk, PAGE_TAG_START );
		pend = std::strstr( chunk, PAGE_TAG_END ); 
		if (pstart != NULL){
			has_page_start = true;
		}
		if ( pend != NULL ){
			has_page_end = true;
			pend += sizeof(char) * strlen( PAGE_TAG_END );
		}

		if( has_page_start && has_page_end ){
			if( pstart < pend){
				page_length = pend - pstart;
				char page[page_length];
				std::strncpy(page, pstart, page_length);
				page[page_length] = '\0';
				pages.push_back(page);
			}
			else {
				has_page_end = false;
			}
			chunk += pend; 
		}
		else if ( !has_page_start && has_page_end ){
			chunk += pend; 
			// Actually this condition is not supposed to happened if the xml file is healthy
		}
		else if ( has_page_start && !has_page_end ){
			chunk += pstart;
			std::strncpy(trail, chunk, std::strlen(chunk));
			trail[std::strlen(chunk)] = '\0';
			break;
		}
		else if ( !has_page_start && !has_page_end ){
			// a tag might be stuck between 2 chunks 
			chunk += std::strlen(chunk) - std::strlen(PAGE_TAG_END);
			std::strncpy(trail, chunk, std::strlen(chunk));
			trail[std::strlen(chunk)] = '\0';
			break;
		}
	}
	return pages;
}


boost::gregorian::date wikitm::get_time(rapidxml::xml_node<> *revision_node){
	boost::gregorian::date d;
	rapidxml::xml_node<> *timestamp_node = revision_node->first_node("timestamp");
	if ( timestamp_node != NULL ){ 
		std::string date_str = timestamp_node->value();
		date_str.erase(10);
		d = boost::gregorian::from_simple_string(date_str);
	}
	return d;
}




std::vector<std::string> wikitm::get_latest_revisions( std::vector<boost::gregorian::date> timeline, 
		std::string page_s ) {
	// TODO  Get the  effective range 
	std::vector<std::string> latest_revisions(timeline.size()); 
	int i_timeline = 0;
	//int i_revisions = 0;

	try {
		rapidxml::xml_document<> page_doc;
		page_doc.parse<0>(&page_s[0]);
		rapidxml::xml_node<> *page_node = page_doc.first_node("page");
		if ( page_node == NULL ) return latest_revisions; // TODO throw an exception

		rapidxml::xml_node<> *ith_revision_node = page_node->first_node(REVISION_TAG_TITLE);
		if ( ( timeline[timeline.size()-1] < get_time(page_node->first_node(REVISION_TAG_TITLE)) ) || 
				( timeline[0] > get_time(page_node->last_node(REVISION_TAG_TITLE)) ) ){
			// timeline is out of the wikipedia range
			return latest_revisions; // TODO throw exception
		}
		while ( timeline[i_timeline] < get_time(ith_revision_node)  
				&& i_timeline < timeline.size() ) {
			latest_revisions[i_timeline] = "";
			i_timeline++;
		}
		while( i_timeline < timeline.size() && ith_revision_node != NULL ) {
			auto ith_revision_time = get_time(ith_revision_node);
			//std::cout << "timeline[" << i_timeline << "]='" << timeline[i_timeline] \
				<< "' ith_revision_time='" << ith_revision_time << "'" << std::endl;
			if ( ith_revision_time > timeline[timeline.size() - 1]){
				if ( ith_revision_node->previous_sibling(REVISION_TAG_TITLE) != NULL ){
					for ( int i = i_timeline; i < timeline.size(); i++){
						std::stringstream ss ;
						ss  << PAGE_TAG_START << std::endl \
							<< *(ith_revision_node->previous_sibling(REVISION_TAG_TITLE)) \
							<< PAGE_TAG_END ; // TODO ith_revision_node->previous_sibling(REVISION_TAG_TITLE)
						latest_revisions[i_timeline] = ss.str();
					}
				}
				else {
					for ( int i = i_timeline; i < timeline.size(); i++){
						latest_revisions[i_timeline] = "";
					}
				}
				break;
			}
			else if( ith_revision_time <= timeline[i_timeline]  ){
				if ( ith_revision_node->next_sibling(REVISION_TAG_TITLE) == NULL ){
					for ( int i = i_timeline; i < timeline.size(); i++){
						std::stringstream ss ;
						ss 	<< PAGE_TAG_START << std::endl \
							<< *ith_revision_node << PAGE_TAG_END ; // TODO ith_revision_node->previous_sibling(REVISION_TAG_TITLE)
						latest_revisions[i_timeline] = ss.str();
					}	
					break;
				}
				else {
					ith_revision_node = ith_revision_node->next_sibling(REVISION_TAG_TITLE);
				}
			}
			else if ( ith_revision_time > timeline[i_timeline] ){
				if ( ith_revision_node->previous_sibling(REVISION_TAG_TITLE) != NULL ){
					std::stringstream ss ;
					ss << PAGE_TAG_START << std::endl << *(ith_revision_node->previous_sibling(REVISION_TAG_TITLE)) << PAGE_TAG_END;
					latest_revisions[i_timeline] = ss.str();
				}
				else {
					for (int i=i_timeline; i >= 0; i--){
						latest_revisions[i_timeline] = "";
					}
				}
				i_timeline++;
			}
		}
	}
	catch ( rapidxml::parse_error& e) {
	  	std::cerr << "Parsing ...failed!" << std::endl;
		std::cerr << "\t" << e.what() << " at " << e.where<std::streampos>(); 
		std::cerr <<  std::endl;
		return latest_revisions;
	}
	return latest_revisions;
}



void wikitm::run(){

	std::cout << "Dumpfiles:" << std::endl;
	for( int i = 0; i < m_dumpfiles.size(); i++){
		std::cout << "\t" << m_dumpfiles[i] << std::endl;
	}
	std::cout << "Timeline:" << std::endl;
	for( int i = 0; i < m_timeline.size(); i++){
		std::cout << "\t" << m_timeline[i] << std::endl;
	}
	std::cout << "Output Files:" << std::endl;
	for( int i = 0; i < m_outfiles.size(); i++){
		std::cout << "\t" << m_outfiles[i] << std::endl;
	}
	
	char* chunk = new char[CHUNK_SIZE];
	for ( int i = 0; i < m_dumpfiles.size(); i++ ){
		long nb_pages_done = 0; // TODO 
		std::vector<std::string> pages;
		long chunks_read = 0;
		std::string chunk_str; 
		std::string prev_chunk; 

		std::cout << "Processing file" << m_dumpfiles[i] << std::endl;
		auto dumpfilepath = m_dumpfiles[i];
		std::time_t start_time = time(0);
		uintmax_t dumpfile_size = fs::file_size(m_dumpfiles[i].c_str()); 
		int file_progress = 0;
		
		std::ifstream fin(dumpfilepath.c_str());

		auto t_file_start = std::chrono::system_clock::now();
		while( true ){
			chunks_read += 1;
			file_progress = ( (long double)(chunks_read*CHUNK_SIZE)/(long double)dumpfile_size ) * 100  ;
			//BOOST_LOG_TRIVIAL(info) << "File " << i+1 << "/" <<  m_dumpfiles.size() 
			std::cout << "File " << i+1 << "/" <<  m_dumpfiles.size() \
				<< " [" << file_progress << "%]" << std::endl;
			//std::cout << nb_pages_done << " total pages done" << std::endl; // TODO useless
			fin.read(chunk, CHUNK_SIZE);
			if ( !fin ) break ;
			chunk_str.assign(chunk, CHUNK_SIZE); // TODO check if chunk not NULL TODO Warning Performance killer 
			pages = find_pages(chunk_str);
			prev_chunk = chunk_str; // TODO check if the chunk is getting too big 
			for ( int i_page = 0; i_page < pages.size(); i_page++ ){
				auto page_revisions = get_latest_revisions(m_timeline, pages[i_page]);
				for (int i_revision = 0; i_revision < page_revisions.size(); i_revision++ ){
					std::ofstream fout;
					fout.open( m_outfiles[i_revision].string() , std::ios_base::app);
					fout << page_revisions[i_revision] << std::endl;
					fout.close();
				}
			}
			nb_pages_done += pages.size();
		}
		auto t_file_end = std::chrono::system_clock::now();
		auto d = t_file_end - t_file_start;
		std::cout << "File " << i+1 << " done in " \
				<< std::chrono::duration_cast<std::chrono::minutes>(d).count() \
				<< "minutes"<< std::endl; 
	}
	delete[] chunk;

}


std::vector<boost::filesystem::path> wikitm::gen_dumplist(std::string filename){
	std::vector<boost::filesystem::path> dumplist;
	fs::path dumpfile(filename);
	dumplist.push_back(dumpfile);
	this->m_dumpfiles = dumplist;
	return dumplist; 
}

std::vector<boost::filesystem::path> wikitm::gen_dumplist_from_folder(std::string input_folder){
	std::vector<boost::filesystem::path> dumplist;
	fs::path dumpdir(input_folder); // TODO solve relative path .., ~
	//fs::path dumpdir = fs::system_complete(input_folder); 
	if ( fs::exists(dumpdir) && fs::is_directory(dumpdir) ) {
		for (fs::directory_entry& f : fs::directory_iterator(dumpdir)){
			//std::cout << f.path().filename().string() << std::endl;
			if (f.path().filename().string().find(DUMPFILE_PREFIX) != std::string::npos ){
				dumplist.push_back(f.path());
			}
		}
	}
	else {
		std::cout << dumpdir.string() << 
			" is not an existing directory" << std::endl;
	}
	this->m_dumpfiles = dumplist;
	return dumplist; 
}


std::vector<boost::filesystem::path> wikitm::gen_dumplist_from_file(std::string input_file_path){
	std::vector<boost::filesystem::path> dumplist;
	std::string line;
	std::ifstream input_file(input_file_path);
	if(input_file.is_open() ) {
		while(!input_file.eof()){
			std::getline(input_file,line);
			fs::path dumpfile(line);
			dumplist.push_back(dumpfile);
		}
	}
	else{
		std::cerr << "Unable to open '" << input_file_path << "'" << std::endl;
	}
	this->m_dumpfiles = dumplist;
	return dumplist; 
}




std::vector<boost::gregorian::date> wikitm::gen_timeline(boost::gregorian::date date_start, 
			boost::gregorian::date_duration date_delta, int date_count) {
	std::vector<boost::gregorian::date> timeline;	
	boost::gregorian::date t = date_start;
	for (int i = 0; i < date_count; i++ ){
		timeline.push_back(t);
		t += date_delta;
	}
	this->m_timeline = timeline;
	return timeline;
}

std::vector<boost::gregorian::date> wikitm::gen_timeline_from_file( std::string timeline_file_path ){
	std::vector<boost::gregorian::date> timeline;	
	std::string line;
	std::ifstream timeline_file(timeline_file_path);
	if(timeline_file.is_open()){
		while(!timeline_file.eof()){
			std::getline(timeline_file,line);
			boost::gregorian::date d(boost::gregorian::from_simple_string( line ));
			timeline.push_back(d);
		}
	}
	else{
		std::cerr << "Unable to open '" << timeline_file_path << "'" << std::endl;
	}
	this->m_timeline = timeline;
	return timeline;
}



std::vector<boost::filesystem::path> wikitm::gen_outfiles(
			const std::vector<boost::gregorian::date>& timeline, 
			const std::string& output_folder){
	std::vector<fs::path> outfiles;
	fs::path outdir(output_folder);
	for ( int i = 0; i < timeline.size(); i++ ){
		auto outfilename = boost::gregorian::to_simple_string(timeline[i]);
		fs::path outfile(outfilename);
		outfiles.push_back( outdir / outfile );
	}
	this->m_outfiles = outfiles;
	return outfiles;
}


std::vector<boost::filesystem::path> wikitm::gen_outfiles( const std::string& output_folder ){
	std::vector<fs::path> outfiles;
	fs::path outdir(output_folder);
	for ( int i = 0; i < this->m_timeline.size(); i++ ){
		auto outfilename = boost::gregorian::to_simple_string(this->m_timeline[i]);
		fs::path outfile(outfilename);
		outfiles.push_back( outdir / outfile );
	}
	this->m_outfiles = outfiles;
	return outfiles;

}


