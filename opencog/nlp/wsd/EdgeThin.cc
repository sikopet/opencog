/*
 * EdgeThin.cc
 *
 * Thin out, remove edges between words, sentences.
 *
 * Copyright (c) 2008 Linas Vepstas <linasvepstas@gmail.com>
 */

#include <opencog/atomspace/Node.h>
#include <opencog/atomspace/TLB.h>

#include "EdgeThin.h"
#include "ForeachWord.h"

#define DEBUG
// #define PRUNE_DEBUG
// #define THIN_DEBUG
// #define LINK_DEBUG
// #define COUNT_DEBUG

using namespace opencog;
/**
 * Remove edges between senses of sets of words
 *
 * The routines in this file operate in a manner similr to those in
 * MihalceaEdge, but "in reverse" -- removing edges instead of adding
 * them.  Most of the routines will have a "count" paramter, indicating
 * the number of edges to keep. The "count" parameter works as follows:
 *
 * If zero, it will remove all edges between all word pairs.
 *
 * If one, it will remove all edges, except the one connecting the
 * top-ranked sense of each word.
 *
 * If two, it will remove all edges that do not connect the top-two
 * ranked senses for each word.
 *
 * If N, it will remove all edges that do not connect the top-N ranked
 * senses for each word.
 */

void EdgeThin::set_atom_space(AtomSpace *as)
{
   atom_space = as;
}

#ifdef PRUNE_DEBUG
bool EdgeThin::count_sense(Handle sense_h, Handle sense_link_h)
{
	sense_count ++;
	return false;
}
#endif

bool EdgeThin::prune_sense(Handle sense_h, Handle sense_link_h)
{
	// If incoming set of this sense link is empty, remove it entirely.
	Atom *a = TLB::getAtom(sense_link_h);
	if (NULL == a->getIncomingSet())
	{
		atom_space->removeAtom(sense_link_h, false);
		prune_count ++;
		return true;
	}
	return false;
}

bool EdgeThin::prune_word(Handle h)
{
	bool rc = true;
	while (rc)
	{
		rc = foreach_word_sense_of_inst(h, &EdgeThin::prune_sense, this);
	}
#ifdef PRUNE_DEBUG
	sense_count = 0;
	foreach_word_sense_of_inst(h, &EdgeThin::count_sense, this);

	Handle wh = get_dict_word_of_word_instance(h);
	Node *n = dynamic_cast<Node *>(TLB::getAtom(wh));
	const char *wd = n->getName().c_str();
	printf("; post-prune, word = %s has %d senses\n", wd, sense_count);
	fflush (stdout);
#endif
	return false;
}

/**
 * Remove all word senses that are not attached to anything.
 * Argument should be a sentence parse.
 */
void EdgeThin::prune_parse(Handle h)
{
	prune_count = 0;
	foreach_word_instance(h, &EdgeThin::prune_word, this);
#ifdef DEBUG
	printf("; EdgeThin::prune_senses pruned %d senses\n", prune_count);
#endif
}

// ===================================================================

#ifdef COUNT_DEBUG
bool EdgeThin::dbg_senses(Handle sense_h, Handle sense_link_h)
{
	Atom *a = TLB::getAtom(sense_link_h);
	if (NULL != a->getIncomingSet())
	{
		Node *w = dynamic_cast<Node *>(TLB::getAtom(sense_h));
		const char* s = w->getName().c_str();
		printf("non-null incoming set for %s\n", s);
	}
	sense_count ++;
	return false;
}

bool EdgeThin::dbg_word(Handle word_h)
{
	sense_count=0;
	foreach_word_sense_of_inst(word_h, &EdgeThin::dbg_senses, this);

	if (0 < sense_count)
	{
		Node *w = dynamic_cast<Node *>(TLB::getAtom(word_h));
		const std::string &wn = w->getName();
		printf ("; EdgeThin::dbg_word %s has %d senses left \n",
			wn.c_str(), sense_count);
	}
	return false;
}

void EdgeThin::dbg_parse(Handle h)
{
	printf ("debug-checking parse %lx\n", h.value());
	foreach_word_instance(h, &EdgeThin::dbg_word, this);
}

#endif

// ===================================================================
/**
 * Remove edges between senses in the indicated parse.
 *
 * Similar to, but opposite of MihalceaEdge::annotate_parse()
 * rather than adding edges, it removes them.
 */
void EdgeThin::thin_parse(Handle h, int _keep)
{
	edge_count = 0;
	keep = _keep;
	foreach_word_instance(h, &EdgeThin::thin_word, this);
#ifdef DEBUG
	printf("; EdgeThin::thin_parse %lx keep=%d deleted %d edges\n",
		h.value(), keep, edge_count);
#endif
}

/**
 * Compare two senses, returning true if the first is more likely than
 * the second.
 */
static bool sense_compare(Handle ha, Handle hb)
{
	Link *la = dynamic_cast<Link *>(TLB::getAtom(ha));
	Link *lb = dynamic_cast<Link *>(TLB::getAtom(hb));
	double sa = la->getTruthValue().getMean();
	double sb = lb->getTruthValue().getMean();
	if (sa > sb) return true;
	return false;
}

bool EdgeThin::make_sense_list(Handle sense_h, Handle sense_link_h)
{
	sense_list.push_back(sense_link_h);
	return false;
}

/**
 * Remove a similarity relation between a pair of word senses.
 * The arg should be a handle to a CosenseLink, previusly constructed
 * by MihalceaEdge::sense_of_second_inst().
 */
bool EdgeThin::delete_sim(Handle h)
{
#ifdef LINK_DEBUG
	Link *l = dynamic_cast<Link *>(TLB::getAtom(h));
	std::vector<Handle> oset = l->getOutgoingSet();
	Handle first_sense_link = oset[0];
	Handle second_sense_link = oset[1];

	Handle fw = get_word_instance_of_sense_link(first_sense_link);
	Handle fs = get_word_sense_of_sense_link(first_sense_link);
	Node *nfw = dynamic_cast<Node *>(TLB::getAtom(fw));
	Node *nfs = dynamic_cast<Node *>(TLB::getAtom(fs));
	const char *vfw = nfw->getName().c_str();
	const char *vfs = nfs->getName().c_str();

	Handle sw = get_word_instance_of_sense_link(second_sense_link);
	Handle ss = get_word_sense_of_sense_link(second_sense_link);
	Node *nsw = dynamic_cast<Node *>(TLB::getAtom(sw));
	Node *nss = dynamic_cast<Node *>(TLB::getAtom(ss));
	const char *vsw = nsw->getName().c_str();
	const char *vss = nss->getName().c_str();

	printf("slink: %s ## %s <<-->> %s ## %s delete\n", vfw, vsw, vfs, vss); 
	printf("slink: %s ## %s <<-->> %s ## %s delete\n", vsw, vfw, vss, vfs); 
#endif

	atom_space->removeAtom(h, false);
	edge_count ++;
	return true;
}

/**
 * Remove edges between senses of a pair of words.
 *
 * Similar to, but opposite to MihalceaEdge::annotate_word_pair()
 * rather than adding edges, it removes them.
 */
bool EdgeThin::thin_word(Handle word_h)
{
	sense_list.clear();
	foreach_word_sense_of_inst(word_h, &EdgeThin::make_sense_list, this);
	sense_list.sort(sense_compare);

#ifdef THIN_DEBUG
	Handle wh = get_dict_word_of_word_instance(word_h);
	Node *w = dynamic_cast<Node *>(TLB::getAtom(wh));
	const std::string &wn = w->getName();
	printf ("; EdgeThin::thin_word %s to %d from %d\n",
		wn.c_str(), keep, sense_list.size());
#endif

	// unqueue the ones that we will keep
	int k = sense_list.size();
	if (keep < k) k = keep;
	for (int i=0; i<k; i++) sense_list.pop_front();

	// Delete the rest.
	std::list<Handle>::iterator it;
	for (it=sense_list.begin(); it != sense_list.end(); it++)
	{
		Handle sense_h = *it;

		// The for-each traversal is not safe against deletion (bummer!)
		// and so has to be restarted after each attempt.
		bool rc = true;
		int deleted_links = 0;
		while (rc)
		{
			rc = foreach_incoming_handle(sense_h, &EdgeThin::delete_sim, this);
			deleted_links ++;
		}
#ifdef THIN_DEBUG
		Link *la = dynamic_cast<Link *>(TLB::getAtom(sense_h));
		double sa = la->getTruthValue().getMean();
		Handle hws = get_word_sense_of_sense_link(sense_h);
		Node *ws = dynamic_cast<Node *>(TLB::getAtom(hws));
		printf ("; delete %s with score %f (%d links)\n",
			ws->getName().c_str(), sa, deleted_links);
#endif
	}

	if (0 >= keep) return false;

	// We can be here only if 'keep' was positive.
	// We'll delete all of the recently disconnected senses. The reason
	// for this is that we don't want these intering with the final score
	// determination process: Some of the disconnected senses might end
	// up with a higher score than the remaining senses, which would be
	// wrong; it would be an inversion of how scoring is meant to work.
	// So we get rid of these now.
	for (it=sense_list.begin(); it != sense_list.end(); it++)
	{
		Handle sense_h = *it;
		atom_space->removeAtom(sense_h, false);
	}

	return false;
}

