/*
   +----------------------------------------------------------------------+
   | HipHop for PHP                                                       |
   +----------------------------------------------------------------------+
   | Copyright (c) 2010-2014 Facebook, Inc. (http://www.facebook.com)     |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
*/
#ifndef incl_HPHP_ALIAS_ANALYSIS_H_
#define incl_HPHP_ALIAS_ANALYSIS_H_

#include <bitset>
#include <string>
#include <cstdint>

#include "hphp/util/sparse-id-containers.h"
#include "hphp/runtime/vm/jit/containers.h"
#include "hphp/runtime/vm/jit/alias-class.h"
#include "hphp/runtime/vm/jit/cfg.h"

namespace HPHP { namespace jit {

struct IRUnit;

//////////////////////////////////////////////////////////////////////

/*
 * Sets of abstract locations tracked by AliasAnalysis come in ALocBits.
 *
 * Right now we have a static maximum number of tracked locations---passes
 * using information from this module must be conservative about locations that
 * aren't assigned an id.  (E.g. via calls to may_alias in AliasAnalysis.)
 */
constexpr uint32_t kMaxTrackedALocs = 128;
using ALocBits = std::bitset<kMaxTrackedALocs>;

//////////////////////////////////////////////////////////////////////

struct ALocMeta {
  uint32_t index;      // id assigned to this location
  ALocBits conflicts;  // flow-insensitive may-alias set, without self
};

/*
 * Information about various abstract locations an IR unit may be concerned
 * with.  See collect_aliases.
 */
struct AliasAnalysis {
  explicit AliasAnalysis(const IRUnit&);

  AliasAnalysis(const AliasAnalysis&) = delete;
  AliasAnalysis(AliasAnalysis&&) = default;
  AliasAnalysis& operator=(const AliasAnalysis&) = delete;
  AliasAnalysis& operator=(AliasAnalysis&&) = default;

  /*
   * Bidirectional map from alias classes to metadata about that abstract
   * memory location, primarily an assigned id.  There is also an inverse map
   * from id to the metadata structure.
   *
   * The keyed locations in this map take their canonical form.  You should use
   * canonicalize before doing lookups.
   */
  jit::hash_map<AliasClass,ALocMeta,AliasClass::Hash> locations;
  jit::vector<ALocMeta> locations_inv;

  /*
   * Some pure store or load instructions affect ranges of stack slots.  If
   * we've assigned all of them ids, they'll have an entry in this map.
   */
  jit::hash_map<AliasClass,ALocBits,AliasClass::Hash> stack_ranges;

  /*
   * Short-hand to find an alias class in the locations map, or get folly::none
   * if the alias class wasn't assigned an ALocMeta structure.
   */
  folly::Optional<ALocMeta> find(AliasClass) const;

  /*
   * Several larger sets of locations, we have a set of all the ids assigned to
   * properties, elemIs, and frame locals.  This is used by may_alias below.
   */
  ALocBits all_props;
  ALocBits all_elemIs;
  ALocBits all_frame;
  ALocBits all_stack;
  ALocBits all_mistate;

  /*
   * Sets of alias classes that are used by must_alias.
   *
   * Note: right now this is only populated for stack locations.  You will have
   * to add more to collect_aliases if you have a new use case.
   */
  jit::hash_map<AliasClass,ALocBits,AliasClass::Hash> stk_must_alias_map;

  /*
   * Return a set of locations that we've assigned ids to that may alias a
   * given AliasClass.  Note that (as usual) memory locations we haven't
   * assigned bits to may still be affected, but this module only reports
   * effects on locations assigned bits.
   */
  ALocBits may_alias(AliasClass acls) const;

  /*
   * Return a set of locations that we've assigned ids to that must be
   * contained in `acls'.  This function will conservatively return an empty
   * set.
   *
   * Right now, this function will work for specific AliasClasses we've
   * assigned ids---for larger classes, it only supports stack ranges observed
   * during alias collection, AFrameAny, and some cases of unions of those---if
   * you need more, it will need some additions.
   */
  ALocBits must_alias(AliasClass acls) const;

  /*
   * Map from frame SSATmp ids to the location bits for all of the frame's
   * locals.
   */
  jit::sparse_idptr_map<SSATmp,ALocBits> per_frame_bits;
};

//////////////////////////////////////////////////////////////////////

/*
 * Perform a flow-insensitive analysis on the supplied blocks, collecting
 * possibly distinct abstract memory locations that are explicitly referenced,
 * and assigning them ids and may-alias sets.  Only certain types of locations
 * are assigned ids, based on whether it maps to an AliasClass that that passes
 * can currently plausibly optimize (because it is sufficiently concrete).
 *
 * Note: it is fine to continue to reuse one AliasAnalysis structure after
 * mutating the IR, because the information it contains is both
 * flow-insensitive and conservative.  That is: if you change the IR to
 * reference new abstract memory locations, the fact that AliasAnalysis didn't
 * know about it won't invalidate the things it knows about (and the general
 * may_alias function will still work on the new location).  Similarly,
 * removing references to locations or changing control flow won't invalidate
 * anything.
 */
AliasAnalysis collect_aliases(const IRUnit&, const BlockList&);

//////////////////////////////////////////////////////////////////////

/*
 * Produce summary information for debug printing.
 */
std::string show(const AliasAnalysis&);
std::string show(ALocBits);

//////////////////////////////////////////////////////////////////////

}}

#endif
