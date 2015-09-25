/* align_seq_raw.cpp
 *
 * Copyright (C) 2009-2014 Nathan Clement
 *
 * This file is part of GNUMAP.
 *
 * GNUMAP is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * GNUMAP is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNUMAP.  If not, see <http://www.gnu.org/licenses/>.
 */
 
bool align_sequence(Genome &gen, seq_map &unique, const Read &search, const string &consensus, 
				double min_align_score, double &denominator, double &top_align_score,
				int strand, int thread_id) {

#ifdef SET_POS
	vector<unsigned long> positions;
#endif

	bin_seq bs;
	
	unsigned int i,j,last_hash=search.length-gMER_SIZE;

	for(i=0; i<last_hash; i+=gJUMP_SIZE) {
		HashLocation* hashes = NULL;
		string consensus_piece;

		// If we've run accross a hash location that is clear (possibly because we've
		// cleared it in a previous step of the algorithm), we want to get another hash
		// that's right next to it instead of jumping over everything.
		for(j=0; j+i<last_hash; j++) {
			consensus_piece = consensus.substr(i+j,gMER_SIZE);
#ifdef DEBUG
			fprintf(stderr,"Consensus piece: >%s<\n",consensus_piece.c_str());
#endif

			// Let's move this into the Genome class
			//p_hash = bs.get_hash(consensus_piece);
			//hashes = gen.GetMatches(p_hash.second);
			hashes = gen.GetMatches(consensus_piece);
			
			// p_hash.first would only be zero if there were n's in the sequence.
			// Just step to a location that doesn't (?) have any n's.
			if(!hashes)  {
#ifdef DEBUG
				//if(gVERBOSE > 2)
					cerr << "Invalid hash sequence at step " << j+i << endl;
#endif
				continue;
			}

			if(hashes->size)	// we've found a valid location
				break;
#ifdef DEBUG
			//if(gVERBOSE > 2)
				cerr << "At location " << j+i << ", no valid hashes found." << endl;
#endif
		} // end of for-loop identifying valid hash location

		// Increment i by j so we don't look here again.
		i+=j;
		
		if(!hashes || !hashes->size) {	// There are no valid locations for this specific hash
#ifdef DEBUG
			//if(gVERBOSE > 2)
				cerr << "No valid hashes for this sequence." << endl;
#endif
			break;	// Go to the next location
		}

#ifdef DEBUG
		cerr << "At location " << i << ", found " << hashes->size << " hashes for this string." << endl;
#endif
		
		for(unsigned int vit = 0; vit<hashes->size; ++vit) {
			//Match the sequence to a sequence that has two characters before and after.
			//string to_match = gen.GetString((*vit)-i-2, search.size()+4);
			
			unsigned long beginning = (hashes->hash_arr[vit]<=i) ? 0 : (hashes->hash_arr[vit]-i);
			
#ifdef SET_POS
			// Check to see if we've already checked this location
			if(gen.getSetThreadID(beginning,thread_id)) {
#ifdef DEBUG
				fprintf(stderr,"position %lu already set\n",beginning);
#endif
				continue;
			}
			else {
				//fprintf(stderr,"position %lu NOT already set\n",beginning);
				positions.push_back(beginning);
			}
			
#endif
				
			
			string to_match = gen.GetString(beginning, search.length);
			
			if(to_match.size() == 0) {	//means it's on a chromosome boundary--not valid sequence
#ifdef DEBUG
				fprintf(stderr,"On chromosome boundary...skipping\n");
#endif
				continue;
			}

			double align_score = 0;
			char aligned[to_match.length()+1];
			strcpy(aligned,to_match.c_str());

			//if(gBISULFITE)
				//align_score = bs.get_align_score_w_traceback(search,to_match,aligned);
			//else
			
#ifdef DEBUG_NW
			// Record the number of nw's this thread performs
			num_nw[thread_id]++;
#endif
			align_score = bs.get_align_score(search,to_match,i,i+gMER_SIZE-1);
			//align_score = bs.get_align_score(search,to_match);

#ifdef DEBUG
//			if(gVERBOSE > 1) 
				cerr << vit << ") " << "matching: " << to_match
					 << "  with  : " << consensus << "\tat pos   " << beginning 
					 							  << "\tand step " << i
												  << "\tand alignment " << align_score 
												  << "\tand min " << min_align_score << endl;
#endif
					 			
			if(align_score > top_align_score)
				top_align_score = align_score;
				
			string unique_string = to_match;
			
			if(align_score >= min_align_score) {
				//fprintf(stderr,"\t**Aligned!\n");
				//if(align_score >= (this_align_score + 4.6))
					//this_align_score = align_score-2;

				// take the rev_c before so we don't need to take it inside the ScoredSeq
				// constructor as well.

				ScoredSeq* temp;
				
				if(gSNP)
					temp = 
						new SNPScoredSeq(to_match, align_score, beginning, strand);
				else if(gBISULFITE || gATOG) {
					temp =
						new BSScoredSeq(to_match,align_score, beginning, strand);
					//fprintf(stderr,"New BSScoredSeq\n");
				}
				else
					temp =
						new NormalScoredSeq(to_match,align_score, beginning, strand);

				if(strand == NEG_STRAND)
					to_match = reverse_comp(to_match);
				
				seq_map::iterator it = unique.find(to_match);
				
				//pair<seq_map::iterator,bool> it_bool = unique.insert(pair<string,ScoredSeq*>(to_match,temp));

				if(it == unique.end()) {	// It wasn't found in the set
					unique.insert(pair<string,ScoredSeq*>(to_match,temp));
					denominator += exp(align_score);
#ifdef DEBUG
					fprintf(stderr,"[%10s%c] just added %s at jump %u with size %lu\n",search.name,strand==POS_STRAND ? '+' : '-',gen.GetPos(beginning,strand).c_str(),i,hashes->size);
					fprintf(stderr,"[%10s%c]     size of unique is now %lu\n",search.name,strand==POS_STRAND?'+':'-',unique.size());
#endif
				}
				else { //if(it != unique.end()) 	// It was found in the set
					
					delete temp;

					if(gUNIQUE) {	// Need to return early because this matches to
									// more than one location
#ifdef SET_POS
						clearPositions(gen,positions,thread_id);
#endif
						return false;
					}


					if( (*it).second->add_spot(beginning,strand) ) {
						//fprintf(stderr,"[%s] %lu:%d Not already here\n",search.name,beginning,strand);
						denominator += exp(align_score);
					}
					//else
						// only doesn't fit if we had to map twice becase of SET_POS flag
						//fprintf(stderr,"[%s] %lu:%d Already in here, sorry\n",search.name,beginning,strand);
#ifdef DEBUG
					fprintf(stderr,"[%10s%c]     Didn't add.  size of unique is now %lu\n",search.name,strand==NEG_STRAND?'+':'-',unique.size());
#endif
				}
				
			}
//			else
//				fprintf(stderr,"\tCould not align with align score %f and min_score %f\n",align_score,min_align_score);
		}
		
		if(unique.size() > gMAX_MATCHES) {
#ifdef SET_POS
			clearPositions(gen,positions,thread_id);
#endif
			return false;
		}
		
		
		// If we want to do the fast mode, only look at one hash location
		if(gFAST)
			break;
	
	}  // end of for over all the hash positions
	
#ifdef SET_POS
	clearPositions(gen,positions,thread_id);
#endif
	return true;
}