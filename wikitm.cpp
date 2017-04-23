//#define BOOST_LOG_DYN_LINK 1

#include "wikitm.hpp"


//using minutes = std::chrono::duration<std::chrono::si29,std::chrono::ratio<60>>;
namespace fs = boost::filesystem;


// Memory size for human reading
// DEPRECATED
std::string memsize_h(long memsize)
{
	std::string t_suffixes[] = {"B", "KB", "MB", "GB", "TB"};
	long m = memsize;
	int s_i = 0;
	while(m > 1024){
		m = m / 1024;
		s_i++;
	}
	if (s_i > 4)
	  return "very big!!";
	//long m_h = memsize / std::pow(1024, s_i);
	std::ostringstream ss;
	//ss <<  m_h << t_suffixes[s_i];
	ss <<  m << t_suffixes[s_i];
	return ss.str(); 
}


std::vector<std::string> wikitm::find_pages(std::string& chunk){
	std::vector<std::string> pages;
	while(true){  
		bool has_page_start = false;
		bool has_page_end = false;
		int i_pstart = chunk.find( PAGE_START );
		int i_pend = chunk.find( PAGE_END );
		if ( i_pstart != std::string::npos ){
			has_page_start = true;
		}
		if ( i_pend != std::string::npos ){
			has_page_end = true;
			i_pend += PAGE_END.size();
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

		rapidxml::xml_node<> *ith_revision_node = page_node->first_node("revision");
		if ( ( timeline[timeline.size()-1] < get_time(page_node->first_node("revision")) ) || 
				( timeline[0] > get_time(page_node->last_node("revision")) ) ){
			// timeline is out of the wikipedia range
			return latest_revisions; // TODO throw exception
		}
		while ( timeline[i_timeline] < get_time(ith_revision_node)  
				&& i_timeline < timeline.size() ) {
			latest_revisions[i_timeline] = "";
			i_timeline++;
		}
		while( i_timeline < timeline.size() ) {
			auto ith_revision_time = get_time(ith_revision_node);
			//std::cout << "timeline[" << i_timeline << "]='" << timeline[i_timeline] \
				<< "' ith_revision_time='" << ith_revision_time << "'" << std::endl;
			if ( ith_revision_time > timeline[timeline.size() - 1]){
				//std::cout << " ====ONE==== " << std::endl;
				if ( ith_revision_node->previous_sibling("revision") != NULL ){
					for ( int i = i_timeline; i < timeline.size(); i++){
						std::stringstream ss ;
						ss  << "<page>" << std::endl \
							<< *(ith_revision_node->previous_sibling("revision")) \
							<< "</page>" ; // TODO ith_revision_node->previous_sibling("revision")
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
				//std::cout << " ====TWO==== " << std::endl;
				if ( ith_revision_node->next_sibling("revision") == NULL ){
					for ( int i = i_timeline; i < timeline.size(); i++){
						std::stringstream ss ;
						ss 	<< "<page>" << std::endl \
							<< *ith_revision_node << "</page>" ; // TODO ith_revision_node->previous_sibling("revision")
						latest_revisions[i_timeline] = ss.str();
					}	
					break;
				}
				else {
					ith_revision_node = ith_revision_node->next_sibling("revision");
				}
			}
			else if ( ith_revision_time > timeline[i_timeline] ){
				//std::cout << " ====THREE==== " << std::endl;
				if ( ith_revision_node->previous_sibling("revision") != NULL ){
					std::stringstream ss ;
					ss << "<page>" << std::endl << *(ith_revision_node->previous_sibling("revision")) << "</page>";
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




void wikitm::wiki_timelapse( boost::gregorian::date date_start, 
		boost::gregorian::date_duration date_delta, int date_count,
		std::string input_folder, std::string output_folder){

	auto dumpfiles = gen_dumplist(input_folder); 
	auto timeline = gen_timeline(date_start, date_delta, date_count);
	auto outfiles = gen_outfiles(timeline, output_folder);

	std::cout << "Dumpfiles:" << std::endl;
	for( int i = 0; i < dumpfiles.size(); i++){
		std::cout << "\t" << dumpfiles[i] << std::endl;
	}
	std::cout << "Timeline:" << std::endl;
	for( int i = 0; i < timeline.size(); i++){
		std::cout << "\t" << timeline[i] << std::endl;
	}
	std::cout << "Output Files:" << std::endl;
	for( int i = 0; i < outfiles.size(); i++){
		std::cout << "\t" << outfiles[i] << std::endl;
	}
	
	for ( int i = 0; i < dumpfiles.size(); i++ ){
		long nb_pages_done = 0; // TODO 
		std::vector<std::string> pages;
		long chunks_read = 0;
		std::string chunk_str; 
		std::string prev_chunk; 

		std::cout << "Processing file" << dumpfiles[i] << std::endl;
		auto dumpfilepath = dumpfiles[i];
		std::time_t start_time = time(0);
		uintmax_t dumpfile_size = fs::file_size(dumpfiles[i].c_str()); 
		int file_progress = 0;
		
		std::ifstream fin(dumpfilepath.c_str());

		auto t_file_start = std::chrono::system_clock::now();
		char* chunk = new char[CHUNK_SIZE];
		while( true ){
			chunks_read += 1;
			file_progress = ( (long double)(chunks_read*CHUNK_SIZE)/(long double)dumpfile_size ) * 100  ;
			//BOOST_LOG_TRIVIAL(info) << "File " << i+1 << "/" <<  dumpfiles.size() 
			std::cout << "File " << i+1 << "/" <<  dumpfiles.size() \
				<< " [" << file_progress << "%]" << std::endl;
			//std::cout << nb_pages_done << " total pages done" << std::endl; // TODO useless
			fin.read(chunk, CHUNK_SIZE);
			if ( !fin ) break ;
			chunk_str.assign(chunk, CHUNK_SIZE); // TODO check if chunk not NULL
			pages = find_pages(chunk_str);
			prev_chunk = chunk_str; // TODO check if the chunk is getting too big 
			for ( int i_page = 0; i_page < pages.size(); i_page++ ){
				auto page_revisions = get_latest_revisions(timeline, pages[i_page]);
				for (int i_revision = 0; i_revision < page_revisions.size(); i_revision++ ){
					std::ofstream fout;
					fout.open( outfiles[i_revision].string() , std::ios_base::app);
					fout << page_revisions[i_revision] << std::endl;
					fout.close();
				}
			}
			nb_pages_done += pages.size();
		}
		delete[] chunk;
		auto t_file_end = std::chrono::system_clock::now();
		auto d = t_file_end - t_file_start;
		std::cout << "File " << i+1 << " done in " \
				<< std::chrono::duration_cast<std::chrono::minutes>(d).count() \
				<< "minutes"<< std::endl; 

	}
}



std::vector<boost::filesystem::path> wikitm::gen_dumplist(std::string input_folder){
	std::vector<boost::filesystem::path> dumplist;
	fs::path dumpdir(input_folder);
	if ( fs::exists(dumpdir) && fs::is_directory(dumpdir) ) {
		for (fs::directory_entry& f : fs::directory_iterator(dumpdir)){
			dumplist.push_back(f.path());
		}
	}
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
	return outfiles;
}




