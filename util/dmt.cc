/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuDB, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <string.h>
#include <db.h>

#include <toku_include/memory.h>
#include <limits.h>

namespace toku {

template<typename dmtdata_t, typename dmtdataout_t>
void dmt<dmtdata_t, dmtdataout_t>::create(void) {
    this->create_internal_no_alloc(false);
    //TODO: maybe allocate enough space for something by default?
    //      We may be relying on not needing to allocate space the first time (due to limited time spent while a lock is held)
}

template<typename dmtdata_t, typename dmtdataout_t>
void dmt<dmtdata_t, dmtdataout_t>::create_from_sorted_memory_of_fixed_size_elements(
        const void *mem,
        const uint32_t numvalues,
        const uint32_t mem_length,
        const uint32_t fixed_value_length) {
    this->values_same_size = true;
    this->value_length = fixed_value_length;
    this->is_array = true;
    this->d.a.start_idx = 0;
    this->d.a.num_values = numvalues;
    const uint8_t pad_bytes = get_fixed_length_alignment_overhead();
    uint32_t aligned_memsize = mem_length + numvalues * pad_bytes;
    toku_mempool_construct(&this->mp, aligned_memsize);
    if (aligned_memsize > 0) {
        void *ptr = toku_mempool_malloc(&this->mp, aligned_memsize, 1);
        paranoid_invariant_notnull(ptr);
        uint8_t *CAST_FROM_VOIDP(dest, ptr);
        const uint8_t *CAST_FROM_VOIDP(src, mem);
        if (pad_bytes == 0) {
            paranoid_invariant(aligned_memsize == mem_length);
            memcpy(dest, src, aligned_memsize);
        } else {
            const uint32_t fixed_len = this->value_length;
            const uint32_t fixed_aligned_len = align(this->value_length);
            paranoid_invariant(this->d.a.num_values*fixed_len == mem_length);
            for (uint32_t i = 0; i < this->d.a.num_values; i++) {
                memcpy(&dest[i*fixed_aligned_len], &src[i*fixed_len], fixed_len);
            }
        }
    }
}

template<typename dmtdata_t, typename dmtdataout_t>
void dmt<dmtdata_t, dmtdataout_t>::create_no_array(void) {
    this->create_internal_no_alloc(false);
}

template<typename dmtdata_t, typename dmtdataout_t>
void dmt<dmtdata_t, dmtdataout_t>::create_internal_no_alloc(bool as_tree) {
    toku_mempool_zero(&this->mp);
    this->values_same_size = true;
    this->value_length = 0;
    this->is_array = !as_tree;
    if (as_tree) {
        this->d.t.root.set_to_null();
    } else {
        this->d.a.start_idx = 0;
        this->d.a.num_values = 0;
    }
}

template<typename dmtdata_t, typename dmtdataout_t>
void dmt<dmtdata_t, dmtdataout_t>::clone(const dmt &src) {
    *this = src;
    toku_mempool_clone(&src.mp, &this->mp);
}

template<typename dmtdata_t, typename dmtdataout_t>
void dmt<dmtdata_t, dmtdataout_t>::clear(void) {
    this->is_array = true;
    this->d.a.start_idx = 0;
    this->d.a.num_values = 0;
    this->values_same_size = true;  // Reset state  //TODO: determine if this is useful.
}

template<typename dmtdata_t, typename dmtdataout_t>
void dmt<dmtdata_t, dmtdataout_t>::destroy(void) {
    this->clear();
    toku_mempool_destroy(&this->mp);
}

template<typename dmtdata_t, typename dmtdataout_t>
uint32_t dmt<dmtdata_t, dmtdataout_t>::size(void) const {
    if (this->is_array) {
        return this->d.a.num_values;
    } else {
        return this->nweight(this->d.t.root);
    }
}

template<typename dmtdata_t, typename dmtdataout_t>
uint32_t dmt<dmtdata_t, dmtdataout_t>::nweight(const subtree &subtree) const {
    if (subtree.is_null()) {
        return 0;
    } else {
        const dmt_base_node & node = get_node<dmt_base_node>(subtree);
        return node.weight;
    }
}

template<typename dmtdata_t, typename dmtdataout_t>
template<typename dmtcmp_t, int (*h)(const uint32_t size, const dmtdata_t &, const dmtcmp_t &)>
int dmt<dmtdata_t, dmtdataout_t>::insert(const dmtdatain_t &value, const dmtcmp_t &v, uint32_t *const idx) {
    int r;
    uint32_t insert_idx;

    r = this->find_zero<dmtcmp_t, h>(v, nullptr, nullptr, &insert_idx);
    if (r==0) {
        if (idx) *idx = insert_idx;
        return DB_KEYEXIST;
    }
    if (r != DB_NOTFOUND) return r;

    if ((r = this->insert_at(value, insert_idx))) return r;
    if (idx) *idx = insert_idx;

    return 0;
}

template<typename dmtdata_t, typename dmtdataout_t>
int dmt<dmtdata_t, dmtdataout_t>::insert_at(const dmtdatain_t &value, const uint32_t idx) {
    if (idx > this->size()) { return EINVAL; }

    bool same_size = this->values_same_size && (this->size() == 0 || value.get_dmtdatain_t_size() == this->value_length);
    if (same_size) {
        if (this->is_array) {
            if (idx == this->d.a.num_values) {
                return this->insert_at_array_end<true>(value);
            }
#if 0
            //TODO: enable if we support delete_at with array
            if (idx == 0 && this->d.a.start_idx > 0) {
                paranoid_invariant(false); // Should not be possible (yet)
                return this->insert_at_array_beginning(value);
            }
#endif
        }
    }
    if (this->is_array) {
        this->convert_to_dtree();
    }
    paranoid_invariant(!is_array);
    paranoid_invariant(!this->values_same_size);
    // Is a d-tree.

    this->maybe_resize_dtree(&value);
    subtree *rebalance_subtree = nullptr;
    this->insert_internal(&this->d.t.root, value, idx, &rebalance_subtree);
    if (rebalance_subtree != nullptr) {
        this->rebalance(rebalance_subtree);
    }
    return 0;
}

template<typename dmtdata_t, typename dmtdataout_t>
template<bool with_resize>
int dmt<dmtdata_t, dmtdataout_t>::insert_at_array_end(const dmtdatain_t& value_in) {
    paranoid_invariant(this->is_array);
    paranoid_invariant(this->values_same_size);
    if (this->d.a.num_values == 0) {
        this->value_length = value_in.get_dmtdatain_t_size();
    }
    paranoid_invariant(this->value_length == value_in.get_dmtdatain_t_size());

    if (with_resize) {
        this->maybe_resize_array(+1);
    }
    dmtdata_t *dest = this->alloc_array_value_end();
    value_in.write_dmtdata_t_to(dest);
    return 0;
}

template<typename dmtdata_t, typename dmtdataout_t>
int dmt<dmtdata_t, dmtdataout_t>::insert_at_array_beginning(const dmtdatain_t& value_in) {
    invariant(false); //TODO: enable this later
    paranoid_invariant(this->is_array);
    paranoid_invariant(this->values_same_size);
    paranoid_invariant(this->d.a.num_values > 0);
    //TODO: when deleting last element, should set start_idx to 0

    this->maybe_resize_array(+1);  // +1 or 0?  Depends on how memory management works
    dmtdata_t *dest = this->alloc_array_value_beginning();
    value_in.write_dmtdata_t_to(dest);
    return 0;
}

template<typename dmtdata_t, typename dmtdataout_t>
dmtdata_t * dmt<dmtdata_t, dmtdataout_t>::alloc_array_value_end(void) {
    paranoid_invariant(this->is_array);
    paranoid_invariant(this->values_same_size);
    this->d.a.num_values++;

    void *ptr = toku_mempool_malloc(&this->mp, align(this->value_length), 1);
    paranoid_invariant_notnull(ptr);
    paranoid_invariant(reinterpret_cast<size_t>(ptr) % ALIGNMENT == 0);
    dmtdata_t *CAST_FROM_VOIDP(n, ptr);
    paranoid_invariant(n == get_array_value(this->d.a.num_values - 1));
    return n;
}

template<typename dmtdata_t, typename dmtdataout_t>
dmtdata_t * dmt<dmtdata_t, dmtdataout_t>::alloc_array_value_beginning(void) {
    invariant(false); //TODO: enable this later
    paranoid_invariant(this->is_array);
    paranoid_invariant(this->values_same_size);

    paranoid_invariant(this->d.a.start_idx > 0);
    const uint32_t real_idx = --this->d.a.start_idx;
    this->d.a.num_values++;
    //TODO: figure out how to keep mempool correct here.. do we free during delete_at (begin)?  If so how do we re'malloc' from beginning?  Alternatively never free from beginning?

    return get_array_value_internal(&this->mp, real_idx);
}

template<typename dmtdata_t, typename dmtdataout_t>
dmtdata_t * dmt<dmtdata_t, dmtdataout_t>::get_array_value(const uint32_t idx) const {
    paranoid_invariant(this->is_array);
    paranoid_invariant(this->values_same_size);

    //TODO: verify initial create always set is_array and values_same_size
    paranoid_invariant(idx < this->d.a.num_values);
    const uint32_t real_idx = idx + this->d.a.start_idx;
    return get_array_value_internal(&this->mp, real_idx);
}

template<typename dmtdata_t, typename dmtdataout_t>
dmtdata_t * dmt<dmtdata_t, dmtdataout_t>::get_array_value_internal(const struct mempool *mempool, const uint32_t real_idx) const {
    void* ptr = toku_mempool_get_pointer_from_base_and_offset(mempool, real_idx * align(this->value_length));
    dmtdata_t *CAST_FROM_VOIDP(value, ptr);
    return value;
}

template<typename dmtdata_t, typename dmtdataout_t>
void dmt<dmtdata_t, dmtdataout_t>::maybe_resize_array(const int change) {
    paranoid_invariant(change == -1 || change == 1);
    paranoid_invariant(change == 1); //TODO: go over change == -1.. may or may not be ok

    bool space_available = change < 0 || toku_mempool_get_free_space(&this->mp) >= align(this->value_length);

    const uint32_t n = this->d.a.num_values + change;
    const uint32_t new_n = n<=2 ? 4 : 2*n;
    const uint32_t new_space = align(this->value_length) * new_n;
    bool too_much_space = new_space <= toku_mempool_get_size(&this->mp) / 2;

    if (!space_available || too_much_space) {
        struct mempool new_kvspace;
        toku_mempool_construct(&new_kvspace, new_space);
        size_t copy_bytes = this->d.a.num_values * align(this->value_length);
        invariant(copy_bytes + align(this->value_length) <= new_space);
        // Copy over to new mempool
        if (this->d.a.num_values > 0) {
            void* dest = toku_mempool_malloc(&new_kvspace, copy_bytes, 1);
            invariant(dest!=nullptr);
            memcpy(dest, get_array_value(0), copy_bytes);
        }
        toku_mempool_destroy(&this->mp);
        this->mp = new_kvspace;
        this->d.a.start_idx = 0;
    }
}

template<typename dmtdata_t, typename dmtdataout_t>
uint32_t dmt<dmtdata_t, dmtdataout_t>::align(const uint32_t x) const {
    return roundup_to_multiple(ALIGNMENT, x);
}

template<typename dmtdata_t, typename dmtdataout_t>
void dmt<dmtdata_t, dmtdataout_t>::convert_to_dtree(void) {
    paranoid_invariant(this->is_array);  //TODO: remove this when ctree implemented
    paranoid_invariant(this->values_same_size);
    if (this->is_array) {
        convert_from_array_to_tree<true>();
    } else {
        //TODO: implement this one.

    }
}

template<typename dmtdata_t, typename dmtdataout_t>
void dmt<dmtdata_t, dmtdataout_t>::prepare_for_serialize(void) {
    if (!this->is_array) {
        this->convert_from_tree_to_array<true>();
    }
}

template<typename dmtdata_t, typename dmtdataout_t>
template<bool with_sizes>
void dmt<dmtdata_t, dmtdataout_t>::convert_from_tree_to_array(void) {
    static_assert(with_sizes, "not in prototype");
    paranoid_invariant(!this->is_array);
    paranoid_invariant(this->values_same_size);
    
    const uint32_t num_values = this->size();

    node_idx *tmp_array;
    bool malloced = false;
    tmp_array = alloc_temp_node_idxs(num_values);
    if (!tmp_array) {
        malloced = true;
        XMALLOC_N(num_values, tmp_array);
    }
    this->fill_array_with_subtree_idxs(tmp_array, this->d.t.root);

    struct mempool new_mp = this->mp;
    const uint32_t fixed_len = this->value_length;
    const uint32_t fixed_aligned_len = align(this->value_length);
    size_t mem_needed = num_values * fixed_aligned_len;
    toku_mempool_construct(&new_mp, mem_needed);
    uint8_t* CAST_FROM_VOIDP(dest, toku_mempool_malloc(&new_mp, mem_needed, 1));
    paranoid_invariant_notnull(dest);
    for (uint32_t i = 0; i < num_values; i++) {
        const dmt_dnode &n = get_node<dmt_dnode>(tmp_array[i]);
        memcpy(&dest[i*fixed_aligned_len], &n.value, fixed_len);
    }
    toku_mempool_destroy(&this->mp);
    this->mp = new_mp;
    this->is_array = true;
    this->d.a.start_idx = 0;
    this->d.a.num_values = num_values;

    if (malloced) toku_free(tmp_array);
}

template<typename dmtdata_t, typename dmtdataout_t>
template<bool with_sizes>
void dmt<dmtdata_t, dmtdataout_t>::convert_from_array_to_tree(void) {
    paranoid_invariant(this->is_array);
    paranoid_invariant(this->values_same_size);
    
    //save array-format information to locals
    const uint32_t num_values = this->d.a.num_values;
    const uint32_t offset = this->d.a.start_idx;
    paranoid_invariant_zero(offset); //TODO: remove this

    static_assert(with_sizes, "not in prototype");

    node_idx *tmp_array;
    bool malloced = false;
    tmp_array = alloc_temp_node_idxs(num_values);
    if (!tmp_array) {
        malloced = true;
        XMALLOC_N(num_values, tmp_array);
    }

    struct mempool old_mp = this->mp;
    size_t mem_needed = num_values * align(this->value_length + __builtin_offsetof(dmt_mnode<with_sizes>, value));
    toku_mempool_construct(&this->mp, mem_needed);

    for (uint32_t i = 0; i < num_values; i++) {
        dmtdatain_t functor(this->value_length, get_array_value_internal(&old_mp, i+offset));
        tmp_array[i] = node_malloc_and_set_value<with_sizes>(functor);
    }
    this->is_array = false;
    this->values_same_size = !with_sizes;
    this->rebuild_subtree_from_idxs(&this->d.t.root, tmp_array, num_values);

    if (malloced) toku_free(tmp_array);
    toku_mempool_destroy(&old_mp);
}

template<typename dmtdata_t, typename dmtdataout_t>
int dmt<dmtdata_t, dmtdataout_t>::delete_at(const uint32_t idx) {
    if (idx >= this->size()) { return EINVAL; }

    //TODO: support array delete/ctree delete
    if (this->is_array) {  //TODO: support ctree
        this->convert_to_dtree();
    }
    paranoid_invariant(!is_array);
    paranoid_invariant(!this->values_same_size);

    if (this->is_array) {
        paranoid_invariant(false);
    } else {
        subtree *rebalance_subtree = nullptr;
        this->delete_internal(&this->d.t.root, idx, nullptr, &rebalance_subtree);
        if (rebalance_subtree != nullptr) {
            this->rebalance(rebalance_subtree);
        }
    }
    this->maybe_resize_dtree(nullptr);
    return 0;
}

template<typename dmtdata_t, typename dmtdataout_t>
template<typename iterate_extra_t,
         int (*f)(const uint32_t, const dmtdata_t &, const uint32_t, iterate_extra_t *const)>
int dmt<dmtdata_t, dmtdataout_t>::iterate(iterate_extra_t *const iterate_extra) const {
    return this->iterate_on_range<iterate_extra_t, f>(0, this->size(), iterate_extra);
}

template<typename dmtdata_t, typename dmtdataout_t>
template<typename iterate_extra_t,
         int (*f)(const uint32_t, const dmtdata_t &, const uint32_t, iterate_extra_t *const)>
int dmt<dmtdata_t, dmtdataout_t>::iterate_on_range(const uint32_t left, const uint32_t right, iterate_extra_t *const iterate_extra) const {
    if (right > this->size()) { return EINVAL; }
    if (left == right) { return 0; }
    if (this->is_array) {
        return this->iterate_internal_array<iterate_extra_t, f>(left, right, iterate_extra);
    }
    return this->iterate_internal<iterate_extra_t, f>(left, right, this->d.t.root, 0, iterate_extra);
}

template<typename dmtdata_t, typename dmtdataout_t>
void dmt<dmtdata_t, dmtdataout_t>::verify(void) const {
    if (!is_array) {
        verify_internal(this->d.t.root);
    }
}

template<typename dmtdata_t, typename dmtdataout_t>
void dmt<dmtdata_t, dmtdataout_t>::verify_internal(const subtree &subtree) const {
    if (subtree.is_null()) {
        return;
    }
    const dmt_dnode &node = get_node<dmt_dnode>(subtree);

    const uint32_t leftweight = this->nweight(node.left);
    const uint32_t rightweight = this->nweight(node.right);

    invariant(leftweight + rightweight + 1 == this->nweight(subtree));
    verify_internal(node.left);
    verify_internal(node.right);
}

template<typename dmtdata_t, typename dmtdataout_t>
template<typename iterate_extra_t,
         int (*f)(const uint32_t, dmtdata_t *, const uint32_t, iterate_extra_t *const)>
void dmt<dmtdata_t, dmtdataout_t>::iterate_ptr(iterate_extra_t *const iterate_extra) {
    if (this->is_array) {
        this->iterate_ptr_internal_array<iterate_extra_t, f>(0, this->size(), iterate_extra);
    } else {
        this->iterate_ptr_internal<iterate_extra_t, f>(0, this->size(), this->d.t.root, 0, iterate_extra);
    }
}

template<typename dmtdata_t, typename dmtdataout_t>
int dmt<dmtdata_t, dmtdataout_t>::fetch(const uint32_t idx, uint32_t *const value_len, dmtdataout_t *const value) const {
    if (idx >= this->size()) { return EINVAL; }
    if (this->is_array) {
        this->fetch_internal_array(idx, value_len, value);
    } else {
        this->fetch_internal(this->d.t.root, idx, value_len, value);
    }
    return 0;
}

template<typename dmtdata_t, typename dmtdataout_t>
template<typename dmtcmp_t,
         int (*h)(const uint32_t, const dmtdata_t &, const dmtcmp_t &)>
int dmt<dmtdata_t, dmtdataout_t>::find_zero(const dmtcmp_t &extra, uint32_t *const value_len, dmtdataout_t *const value, uint32_t *const idxp) const {
    uint32_t tmp_index;
    uint32_t *const child_idxp = (idxp != nullptr) ? idxp : &tmp_index;
    int r;
    if (this->is_array) {
        r = this->find_internal_zero_array<dmtcmp_t, h>(extra, value_len, value, child_idxp);
    }
    else {
        r = this->find_internal_zero<dmtcmp_t, h>(this->d.t.root, extra, value_len, value, child_idxp);
    }
    return r;
}

template<typename dmtdata_t, typename dmtdataout_t>
template<typename dmtcmp_t,
         int (*h)(const uint32_t, const dmtdata_t &, const dmtcmp_t &)>
int dmt<dmtdata_t, dmtdataout_t>::find(const dmtcmp_t &extra, int direction, uint32_t *const value_len, dmtdataout_t *const value, uint32_t *const idxp) const {
    uint32_t tmp_index;
    uint32_t *const child_idxp = (idxp != nullptr) ? idxp : &tmp_index;
    paranoid_invariant(direction != 0);
    if (direction < 0) {
        if (this->is_array) {
            return this->find_internal_minus_array<dmtcmp_t, h>(extra, value_len,  value, child_idxp);
        } else {
            return this->find_internal_minus<dmtcmp_t, h>(this->d.t.root, extra, value_len,  value, child_idxp);
        }
    } else {
        if (this->is_array) {
            return this->find_internal_plus_array<dmtcmp_t, h>(extra, value_len,  value, child_idxp);
        } else {
            return this->find_internal_plus<dmtcmp_t, h>(this->d.t.root, extra, value_len, value, child_idxp);
        }
    }
}

template<typename dmtdata_t, typename dmtdataout_t>
size_t dmt<dmtdata_t, dmtdataout_t>::memory_size(void) {
    return (sizeof *this) + toku_mempool_get_size(&this->mp);
}

template<typename dmtdata_t, typename dmtdataout_t>
template<typename node_type>
node_type & dmt<dmtdata_t, dmtdataout_t>::get_node(const subtree &subtree) const {
    paranoid_invariant(!subtree.is_null());
    return get_node<node_type>(subtree.get_index());
}

template<typename dmtdata_t, typename dmtdataout_t>
template<typename node_type>
node_type & dmt<dmtdata_t, dmtdataout_t>::get_node(const node_idx offset) const {
    //TODO: implement
    //Need to decide what to do with regards to cnode/dnode
    void* ptr = toku_mempool_get_pointer_from_base_and_offset(&this->mp, offset);
    node_type *CAST_FROM_VOIDP(node, ptr);
    return *node;
}

template<typename dmtdata_t, typename dmtdataout_t>
void dmt<dmtdata_t, dmtdataout_t>::node_set_value(dmt_mnode<true> * n, const dmtdatain_t &value) {
    n->value_length = value.get_dmtdatain_t_size();
    value.write_dmtdata_t_to(&n->value);
}

template<typename dmtdata_t, typename dmtdataout_t>
template<bool with_length>
node_idx dmt<dmtdata_t, dmtdataout_t>::node_malloc_and_set_value(const dmtdatain_t &value) {
    static_assert(with_length, "not in prototype");
    size_t val_size = value.get_dmtdatain_t_size();
    size_t size_to_alloc = __builtin_offsetof(dmt_mnode<with_length>, value) + val_size;
    size_to_alloc = align(size_to_alloc);
    void* np = toku_mempool_malloc(&this->mp, size_to_alloc, 1);
    paranoid_invariant_notnull(np);
    dmt_mnode<with_length> *CAST_FROM_VOIDP(n, np);
    node_set_value(n, value);

    n->b.clear_stolen_bits();
    return toku_mempool_get_offset_from_pointer_and_base(&this->mp, np);
}

template<typename dmtdata_t, typename dmtdataout_t>
void dmt<dmtdata_t, dmtdataout_t>::node_free(const subtree &st) {
    dmt_dnode &n = get_node<dmt_dnode>(st);
    size_t size_to_free = __builtin_offsetof(dmt_dnode, value) + n.value_length;
    size_to_free = align(size_to_free);
    toku_mempool_mfree(&this->mp, &n, size_to_free);
}

template<typename dmtdata_t, typename dmtdataout_t>
void dmt<dmtdata_t, dmtdataout_t>::maybe_resize_dtree(const dmtdatain_t * value) {
    static_assert(std::is_same<dmtdatain_t, dmtdatain_t>::value, "functor wrong type");
    const ssize_t curr_capacity = toku_mempool_get_size(&this->mp);
    const ssize_t curr_free = toku_mempool_get_free_space(&this->mp);
    const ssize_t curr_used = toku_mempool_get_used_space(&this->mp);
    ssize_t add_size = 0;
    if (value) {
        add_size = __builtin_offsetof(dmt_dnode, value) + value->get_dmtdatain_t_size();
        add_size = align(add_size);
    }

    const ssize_t need_size = curr_used + add_size;
    paranoid_invariant(need_size <= UINT32_MAX);
    const ssize_t new_size = 2*need_size;
    paranoid_invariant(new_size <= UINT32_MAX);

    //const uint32_t num_nodes = this->nweight(this->d.t.root);

    if ((curr_capacity / 2 >= new_size) || // Way too much allocated
        (curr_free < add_size)) {  // No room in mempool
        // Copy all memory and reconstruct dmt in new mempool.
        struct mempool new_kvspace;
        struct mempool old_kvspace;
        toku_mempool_construct(&new_kvspace, new_size);

        if (!this->d.t.root.is_null()) {
            const dmt_dnode &n = get_node<dmt_dnode>(this->d.t.root);
            node_idx *tmp_array;
            bool malloced = false;
            tmp_array = alloc_temp_node_idxs(n.b.weight);
            if (!tmp_array) {
                malloced = true;
                XMALLOC_N(n.b.weight, tmp_array);
            }
            this->fill_array_with_subtree_idxs(tmp_array, this->d.t.root);
            for (node_idx i = 0; i < n.b.weight; i++) {
                dmt_dnode &node = get_node<dmt_dnode>(tmp_array[i]);
                const size_t bytes_to_copy = __builtin_offsetof(dmt_dnode, value) + node.value_length;
                const size_t bytes_to_alloc = align(bytes_to_copy);
                void* newdata = toku_mempool_malloc(&new_kvspace, bytes_to_alloc, 1);
                memcpy(newdata, &node, bytes_to_copy);
                tmp_array[i] = toku_mempool_get_offset_from_pointer_and_base(&new_kvspace, newdata);
            }

            old_kvspace = this->mp;
            this->mp = new_kvspace;
            this->rebuild_subtree_from_idxs(&this->d.t.root, tmp_array, n.b.weight);
            if (malloced) toku_free(tmp_array);
            toku_mempool_destroy(&old_kvspace);
        }
        else {
            toku_mempool_destroy(&this->mp);
            this->mp = new_kvspace;
        }
    }
}

template<typename dmtdata_t, typename dmtdataout_t>
bool dmt<dmtdata_t, dmtdataout_t>::will_need_rebalance(const subtree &subtree, const int leftmod, const int rightmod) const {
    if (subtree.is_null()) { return false; }
    const dmt_dnode &n = get_node<dmt_dnode>(subtree);
    // one of the 1's is for the root.
    // the other is to take ceil(n/2)
    const uint32_t weight_left  = this->nweight(n.b.left)  + leftmod;
    const uint32_t weight_right = this->nweight(n.b.right) + rightmod;
    return ((1+weight_left < (1+1+weight_right)/2)
            ||
            (1+weight_right < (1+1+weight_left)/2));
}

template<typename dmtdata_t, typename dmtdataout_t>
void dmt<dmtdata_t, dmtdataout_t>::insert_internal(subtree *const subtreep, const dmtdatain_t &value, const uint32_t idx, subtree **const rebalance_subtree) {
    if (subtreep->is_null()) {
        paranoid_invariant_zero(idx);
        const node_idx newidx = this->node_malloc_and_set_value<true>(value);  //TODO: make this not <true> arbitrarily
        dmt_dnode &newnode = get_node<dmt_dnode>(newidx);
        newnode.b.weight = 1;
        newnode.b.left.set_to_null();
        newnode.b.right.set_to_null();
        subtreep->set_index(newidx);
    } else {
        dmt_dnode &n = get_node<dmt_dnode>(*subtreep);
        n.b.weight++;
        if (idx <= this->nweight(n.b.left)) {
            if (*rebalance_subtree == nullptr && this->will_need_rebalance(*subtreep, 1, 0)) {
                *rebalance_subtree = subtreep;
            }
            this->insert_internal(&n.b.left, value, idx, rebalance_subtree);
        } else {
            if (*rebalance_subtree == nullptr && this->will_need_rebalance(*subtreep, 0, 1)) {
                *rebalance_subtree = subtreep;
            }
            const uint32_t sub_index = idx - this->nweight(n.b.left) - 1;
            this->insert_internal(&n.b.right, value, sub_index, rebalance_subtree);
        }
    }
}

template<typename dmtdata_t, typename dmtdataout_t>
void dmt<dmtdata_t, dmtdataout_t>::delete_internal(subtree *const subtreep, const uint32_t idx, subtree *const subtree_replace, subtree **const rebalance_subtree) {
    paranoid_invariant_notnull(subtreep);
    paranoid_invariant_notnull(rebalance_subtree);
    paranoid_invariant(!subtreep->is_null());
    dmt_dnode &n = get_node<dmt_dnode>(*subtreep);
    const uint32_t leftweight = this->nweight(n.b.left);
    if (idx < leftweight) {
        n.b.weight--;
        if (*rebalance_subtree == nullptr && this->will_need_rebalance(*subtreep, -1, 0)) {
            *rebalance_subtree = subtreep;
        }
        this->delete_internal(&n.b.left, idx, subtree_replace, rebalance_subtree);
    } else if (idx == leftweight) {
        // Found the correct index.
        if (n.b.left.is_null()) {
            // Delete n and let parent point to n.b.right
            subtree ptr_this = *subtreep;
            *subtreep = n.b.right;
            subtree to_free;
            if (subtree_replace != nullptr) {
                // Swap self with the other node.
                to_free = *subtree_replace;
                dmt_dnode &ancestor = get_node<dmt_dnode>(*subtree_replace);
                if (*rebalance_subtree == &ancestor.b.right) {
                    // Take over rebalance responsibility.
                    *rebalance_subtree = &n.b.right;
                }
                n.b.weight = ancestor.b.weight;
                n.b.left = ancestor.b.left;
                n.b.right = ancestor.b.right;
                *subtree_replace = ptr_this;
            } else {
                to_free = ptr_this;
            }
            this->node_free(to_free);
        } else if (n.b.right.is_null()) {
            // Delete n and let parent point to n.b.left
            subtree to_free = *subtreep;
            *subtreep = n.b.left;
            paranoid_invariant_null(subtree_replace);  // To be recursive, we're looking for index 0.  n is index > 0 here.
            this->node_free(to_free);
        } else {
            if (*rebalance_subtree == nullptr && this->will_need_rebalance(*subtreep, 0, -1)) {
                *rebalance_subtree = subtreep;
            }
            // don't need to copy up value, it's only used by this
            // next call, and when that gets to the bottom there
            // won't be any more recursion
            n.b.weight--;
            this->delete_internal(&n.b.right, 0, subtreep, rebalance_subtree);
        }
    } else {
        n.b.weight--;
        if (*rebalance_subtree == nullptr && this->will_need_rebalance(*subtreep, 0, -1)) {
            *rebalance_subtree = subtreep;
        }
        this->delete_internal(&n.b.right, idx - leftweight - 1, subtree_replace, rebalance_subtree);
    }
}

template<typename dmtdata_t, typename dmtdataout_t>
template<typename iterate_extra_t,
         int (*f)(const uint32_t, const dmtdata_t &, const uint32_t, iterate_extra_t *const)>
int dmt<dmtdata_t, dmtdataout_t>::iterate_internal_array(const uint32_t left, const uint32_t right,
                                                         iterate_extra_t *const iterate_extra) const {
    int r;
    for (uint32_t i = left; i < right; ++i) {
        r = f(this->value_length, *get_array_value(i), i, iterate_extra);
        if (r != 0) {
            return r;
        }
    }
    return 0;
}

template<typename dmtdata_t, typename dmtdataout_t>
template<typename iterate_extra_t,
         int (*f)(const uint32_t, dmtdata_t *, const uint32_t, iterate_extra_t *const)>
void dmt<dmtdata_t, dmtdataout_t>::iterate_ptr_internal(const uint32_t left, const uint32_t right,
                                                        const subtree &subtree, const uint32_t idx,
                                                        iterate_extra_t *const iterate_extra) {
    if (!subtree.is_null()) { 
        dmt_dnode &n = get_node<dmt_dnode>(subtree);
        const uint32_t idx_root = idx + this->nweight(n.b.left);
        if (left < idx_root) {
            this->iterate_ptr_internal<iterate_extra_t, f>(left, right, n.b.left, idx, iterate_extra);
        }
        if (left <= idx_root && idx_root < right) {
            int r = f(n.value_length, &n.value, idx_root, iterate_extra);
            lazy_assert_zero(r);
        }
        if (idx_root + 1 < right) {
            this->iterate_ptr_internal<iterate_extra_t, f>(left, right, n.b.right, idx_root + 1, iterate_extra);
        }
    }
}

template<typename dmtdata_t, typename dmtdataout_t>
template<typename iterate_extra_t,
         int (*f)(const uint32_t, dmtdata_t *, const uint32_t, iterate_extra_t *const)>
void dmt<dmtdata_t, dmtdataout_t>::iterate_ptr_internal_array(const uint32_t left, const uint32_t right,
                                                              iterate_extra_t *const iterate_extra) {
    for (uint32_t i = left; i < right; ++i) {
        int r = f(this->value_length, get_array_value(i), i, iterate_extra);
        lazy_assert_zero(r);
    }
}

template<typename dmtdata_t, typename dmtdataout_t>
template<typename iterate_extra_t,
         int (*f)(const uint32_t, const dmtdata_t &, const uint32_t, iterate_extra_t *const)>
int dmt<dmtdata_t, dmtdataout_t>::iterate_internal(const uint32_t left, const uint32_t right,
                                                   const subtree &subtree, const uint32_t idx,
                                                   iterate_extra_t *const iterate_extra) const {
    if (subtree.is_null()) { return 0; }
    int r;
    const dmt_dnode &n = get_node<dmt_dnode>(subtree);
    const uint32_t idx_root = idx + this->nweight(n.b.left);
    if (left < idx_root) {
        r = this->iterate_internal<iterate_extra_t, f>(left, right, n.b.left, idx, iterate_extra);
        if (r != 0) { return r; }
    }
    if (left <= idx_root && idx_root < right) {
        r = f(n.value_length, n.value, idx_root, iterate_extra);
        if (r != 0) { return r; }
    }
    if (idx_root + 1 < right) {
        return this->iterate_internal<iterate_extra_t, f>(left, right, n.b.right, idx_root + 1, iterate_extra);
    }
    return 0;
}

template<typename dmtdata_t, typename dmtdataout_t>
void dmt<dmtdata_t, dmtdataout_t>::fetch_internal_array(const uint32_t i, uint32_t *const value_len, dmtdataout_t *const value) const {
    copyout(value_len, value, this->value_length, get_array_value(i));
}

template<typename dmtdata_t, typename dmtdataout_t>
void dmt<dmtdata_t, dmtdataout_t>::fetch_internal(const subtree &subtree, const uint32_t i, uint32_t *const value_len, dmtdataout_t *const value) const {
    dmt_dnode &n = get_node<dmt_dnode>(subtree);
    const uint32_t leftweight = this->nweight(n.b.left);
    if (i < leftweight) {
        this->fetch_internal(n.b.left, i, value_len, value);
    } else if (i == leftweight) {
        copyout(value_len, value, &n);
    } else {
        this->fetch_internal(n.b.right, i - leftweight - 1, value_len, value);
    }
}

template<typename dmtdata_t, typename dmtdataout_t>
void dmt<dmtdata_t, dmtdataout_t>::fill_array_with_subtree_idxs(node_idx *const array, const subtree &subtree) const {
    if (!subtree.is_null()) {
        const dmt_dnode &tree = get_node<dmt_dnode>(subtree);
        this->fill_array_with_subtree_idxs(&array[0], tree.b.left);
        array[this->nweight(tree.b.left)] = subtree.get_index();
        this->fill_array_with_subtree_idxs(&array[this->nweight(tree.b.left) + 1], tree.b.right);
    }
}

template<typename dmtdata_t, typename dmtdataout_t>
void dmt<dmtdata_t, dmtdataout_t>::rebuild_subtree_from_idxs(subtree *const subtree, const node_idx *const idxs, const uint32_t numvalues) {
    if (numvalues==0) {
        subtree->set_to_null();
    } else {
        uint32_t halfway = numvalues/2;
        subtree->set_index(idxs[halfway]);
        dmt_dnode &newnode = get_node<dmt_dnode>(idxs[halfway]);
        newnode.b.weight = numvalues;
        // value is already in there.
        this->rebuild_subtree_from_idxs(&newnode.b.left,  &idxs[0], halfway);
        this->rebuild_subtree_from_idxs(&newnode.b.right, &idxs[halfway+1], numvalues-(halfway+1));
    }
}

template<typename dmtdata_t, typename dmtdataout_t>
node_idx* dmt<dmtdata_t, dmtdataout_t>::alloc_temp_node_idxs(uint32_t num_idxs) {
    size_t mem_needed = num_idxs * sizeof(node_idx);
    size_t mem_free;
    mem_free = toku_mempool_get_free_space(&this->mp);
    node_idx* CAST_FROM_VOIDP(tmp, toku_mempool_get_next_free_ptr(&this->mp));
    if (mem_free >= mem_needed) {
        return tmp;
    }
    return nullptr;
}

template<typename dmtdata_t, typename dmtdataout_t>
void dmt<dmtdata_t, dmtdataout_t>::rebalance(subtree *const subtree) {
    (void) subtree;
    paranoid_invariant(!subtree->is_null());
    node_idx idx = subtree->get_index();
//TODO: restore optimization
#if 0
    if (!dynamic && idx==this->d.t.root.get_index()) {
        //Try to convert to an array.
        //If this fails, (malloc) nothing will have changed.
        //In the failure case we continue on to the standard rebalance
        //algorithm.
        this->convert_to_array();
        if (supports_marks) {
            this->convert_to_tree();
        }
    } else {
#endif
        const dmt_dnode &n = get_node<dmt_dnode>(idx);
        node_idx *tmp_array;
        bool malloced = false;
        tmp_array = alloc_temp_node_idxs(n.b.weight);
        if (!tmp_array) {
            malloced = true;
            XMALLOC_N(n.b.weight, tmp_array);
        }
        this->fill_array_with_subtree_idxs(tmp_array, *subtree);
        this->rebuild_subtree_from_idxs(subtree, tmp_array, n.b.weight);
        if (malloced) toku_free(tmp_array);
#if 0
    }
#endif
}

template<typename dmtdata_t, typename dmtdataout_t>
void dmt<dmtdata_t, dmtdataout_t>::copyout(uint32_t *const outlen, dmtdata_t *const out, const dmt_dnode *const n) {
    if (outlen) {
        *outlen = n->value_length;
    }
    if (out) {
        *out = n->value;
    }
}

template<typename dmtdata_t, typename dmtdataout_t>
void dmt<dmtdata_t, dmtdataout_t>::copyout(uint32_t *const outlen, dmtdata_t **const out, dmt_dnode *const n) {
    if (outlen) {
        *outlen = n->value_length;
    }
    if (out) {
        *out = &n->value;
    }
}

template<typename dmtdata_t, typename dmtdataout_t>
void dmt<dmtdata_t, dmtdataout_t>::copyout(uint32_t *const outlen, dmtdata_t *const out, const uint32_t len, const dmtdata_t *const stored_value_ptr) {
    if (outlen) {
        *outlen = len;
    }
    if (out) {
        *out = *stored_value_ptr;
    }
}

template<typename dmtdata_t, typename dmtdataout_t>
void dmt<dmtdata_t, dmtdataout_t>::copyout(uint32_t *const outlen, dmtdata_t **const out, const uint32_t len, dmtdata_t *const stored_value_ptr) {
    if (outlen) {
        *outlen = len;
    }
    if (out) {
        *out = stored_value_ptr;
    }
}

template<typename dmtdata_t, typename dmtdataout_t>
template<typename dmtcmp_t,
         int (*h)(const uint32_t, const dmtdata_t &, const dmtcmp_t &)>
int dmt<dmtdata_t, dmtdataout_t>::find_internal_zero_array(const dmtcmp_t &extra, uint32_t *const value_len, dmtdataout_t *const value, uint32_t *const idxp) const {
    paranoid_invariant_notnull(idxp);
    uint32_t min = 0;
    uint32_t limit = this->d.a.num_values;
    uint32_t best_pos = subtree::NODE_NULL;
    uint32_t best_zero = subtree::NODE_NULL;

    while (min!=limit) {
        uint32_t mid = (min + limit) / 2;
        int hv = h(this->value_length, *get_array_value(mid), extra);
        if (hv<0) {
            min = mid+1;
        }
        else if (hv>0) {
            best_pos  = mid;
            limit     = mid;
        }
        else {
            best_zero = mid;
            limit     = mid;
        }
    }
    if (best_zero!=subtree::NODE_NULL) {
        //Found a zero
        copyout(value_len, value, this->value_length, get_array_value(best_zero));
        *idxp = best_zero;
        return 0;
    }
    if (best_pos!=subtree::NODE_NULL) *idxp = best_pos;
    else                     *idxp = this->d.a.num_values;
    return DB_NOTFOUND;
}

template<typename dmtdata_t, typename dmtdataout_t>
template<typename dmtcmp_t,
         int (*h)(const uint32_t, const dmtdata_t &, const dmtcmp_t &)>
int dmt<dmtdata_t, dmtdataout_t>::find_internal_zero(const subtree &subtree, const dmtcmp_t &extra, uint32_t *const value_len, dmtdataout_t *const value, uint32_t *const idxp) const {
    paranoid_invariant_notnull(idxp);
    if (subtree.is_null()) {
        *idxp = 0;
        return DB_NOTFOUND;
    }
    dmt_dnode &n = get_node<dmt_dnode>(subtree);
    int hv = h(n.value_length, n.value, extra);
    if (hv<0) {
        int r = this->find_internal_zero<dmtcmp_t, h>(n.b.right, extra, value_len, value, idxp);
        *idxp += this->nweight(n.b.left)+1;
        return r;
    } else if (hv>0) {
        return this->find_internal_zero<dmtcmp_t, h>(n.b.left, extra, value_len, value, idxp);
    } else {
        int r = this->find_internal_zero<dmtcmp_t, h>(n.b.left, extra, value_len, value, idxp);
        if (r==DB_NOTFOUND) {
            *idxp = this->nweight(n.b.left);
            copyout(value_len, value, &n);
            r = 0;
        }
        return r;
    }
}

template<typename dmtdata_t, typename dmtdataout_t>
template<typename dmtcmp_t,
         int (*h)(const uint32_t, const dmtdata_t &, const dmtcmp_t &)>
int dmt<dmtdata_t, dmtdataout_t>::find_internal_plus_array(const dmtcmp_t &extra, uint32_t *const value_len, dmtdataout_t *const value, uint32_t *const idxp) const {
    paranoid_invariant_notnull(idxp);
    uint32_t min = 0;
    uint32_t limit = this->d.a.num_values;
    uint32_t best = subtree::NODE_NULL;

    while (min != limit) {
        const uint32_t mid = (min + limit) / 2;
        const int hv = h(this->value_length, *get_array_value(mid), extra);
        if (hv > 0) {
            best = mid;
            limit = mid;
        } else {
            min = mid + 1;
        }
    }
    if (best == subtree::NODE_NULL) { return DB_NOTFOUND; }
    copyout(value_len, value, this->value_length, get_array_value(best));
    *idxp = best;
    return 0;
}

template<typename dmtdata_t, typename dmtdataout_t>
template<typename dmtcmp_t,
         int (*h)(const uint32_t, const dmtdata_t &, const dmtcmp_t &)>
int dmt<dmtdata_t, dmtdataout_t>::find_internal_plus(const subtree &subtree, const dmtcmp_t &extra, uint32_t *const value_len, dmtdataout_t *const value, uint32_t *const idxp) const {
    paranoid_invariant_notnull(idxp);
    if (subtree.is_null()) {
        return DB_NOTFOUND;
    }
    dmt_dnode & n = get_node<dmt_dnode>(subtree);
    int hv = h(n.value_length, n.value, extra);
    int r;
    if (hv > 0) {
        r = this->find_internal_plus<dmtcmp_t, h>(n.b.left, extra, value_len, value, idxp);
        if (r == DB_NOTFOUND) {
            *idxp = this->nweight(n.b.left);
            copyout(value_len, value, &n);
            r = 0;
        }
    } else {
        r = this->find_internal_plus<dmtcmp_t, h>(n.b.right, extra, value_len, value, idxp);
        if (r == 0) {
            *idxp += this->nweight(n.b.left) + 1;
        }
    }
    return r;
}

template<typename dmtdata_t, typename dmtdataout_t>
template<typename dmtcmp_t,
         int (*h)(const uint32_t, const dmtdata_t &, const dmtcmp_t &)>
int dmt<dmtdata_t, dmtdataout_t>::find_internal_minus_array(const dmtcmp_t &extra, uint32_t *const value_len, dmtdataout_t *const value, uint32_t *const idxp) const {
    paranoid_invariant_notnull(idxp);
    uint32_t min = 0;
    uint32_t limit = this->d.a.num_values;
    uint32_t best = subtree::NODE_NULL;

    while (min != limit) {
        const uint32_t mid = (min + limit) / 2;
        const int hv = h(this->value_length, *get_array_value(mid), extra);
        if (hv < 0) {
            best = mid;
            min = mid + 1;
        } else {
            limit = mid;
        }
    }
    if (best == subtree::NODE_NULL) { return DB_NOTFOUND; }
    copyout(value_len, value, this->value_length, get_array_value(best));
    *idxp = best;
    return 0;
}

template<typename dmtdata_t, typename dmtdataout_t>
template<typename dmtcmp_t,
         int (*h)(const uint32_t, const dmtdata_t &, const dmtcmp_t &)>
int dmt<dmtdata_t, dmtdataout_t>::find_internal_minus(const subtree &subtree, const dmtcmp_t &extra, uint32_t *const value_len, dmtdataout_t *const value, uint32_t *const idxp) const {
    paranoid_invariant_notnull(idxp);
    if (subtree.is_null()) {
        return DB_NOTFOUND;
    }
    dmt_dnode & n = get_node<dmt_dnode>(subtree);
    int hv = h(n.value_length, n.value, extra);
    if (hv < 0) {
        int r = this->find_internal_minus<dmtcmp_t, h>(n.b.right, extra, value_len, value, idxp);
        if (r == 0) {
            *idxp += this->nweight(n.b.left) + 1;
        } else if (r == DB_NOTFOUND) {
            *idxp = this->nweight(n.b.left);
            copyout(value_len, value, &n);
            r = 0;
        }
        return r;
    } else {
        return this->find_internal_minus<dmtcmp_t, h>(n.b.left, extra, value_len, value, idxp);
    }
}

template<typename dmtdata_t, typename dmtdataout_t>
uint32_t dmt<dmtdata_t, dmtdataout_t>::get_fixed_length(void) const {
    return this->values_same_size ? this->value_length : 0;
}

template<typename dmtdata_t, typename dmtdataout_t>
uint32_t dmt<dmtdata_t, dmtdataout_t>::get_fixed_length_alignment_overhead(void) const {
    return this->values_same_size ? align(this->value_length) - this->value_length : 0;
}

template<typename dmtdata_t, typename dmtdataout_t>
bool dmt<dmtdata_t, dmtdataout_t>::is_value_length_fixed(void) const {
    return this->values_same_size;
}

template<typename dmtdata_t, typename dmtdataout_t>
const struct mempool * dmt<dmtdata_t, dmtdataout_t>::serialize_values(uint32_t expected_unpadded_memory, struct wbuf *wb) const {
    invariant(this->is_array);
    const uint8_t pad_bytes = get_fixed_length_alignment_overhead();
    const uint32_t fixed_len = this->value_length;
    const uint32_t fixed_aligned_len = align(this->value_length);
    paranoid_invariant(expected_unpadded_memory == this->d.a.num_values * this->value_length);
    paranoid_invariant(toku_mempool_get_used_space(&this->mp) >=
                       expected_unpadded_memory + pad_bytes * this->d.a.num_values +
                       this->d.a.start_idx * fixed_aligned_len);
    if (this->d.a.num_values == 0) {
        // Nothing to serialize
    } else if (pad_bytes == 0) {
        // Basically a memcpy
        wbuf_nocrc_literal_bytes(wb, get_array_value(0), expected_unpadded_memory);
    } else {
        uint8_t* dest = wbuf_nocrc_reserve_literal_bytes(wb, expected_unpadded_memory);
        uint8_t* src = reinterpret_cast<uint8_t*>(get_array_value(0));
        paranoid_invariant(this->d.a.num_values*fixed_len == expected_unpadded_memory);
        for (uint32_t i = 0; i < this->d.a.num_values; i++) {
            memcpy(&dest[i*fixed_len], &src[i*fixed_aligned_len], fixed_len);
        }
    }

    return &this->mp;
}

template<typename dmtdata_t, typename dmtdataout_t>
void dmt<dmtdata_t, dmtdataout_t>::builder::create(uint32_t _max_values, uint32_t _max_value_bytes) {
    this->max_values = _max_values;
    this->max_value_bytes = _max_value_bytes;
    this->temp.create_no_array();
    this->temp_valid = true;
    this->sorted_nodes = nullptr;
    // Include enough space for alignment padding
    size_t initial_space = (ALIGNMENT - 1) * _max_values + _max_value_bytes;

    toku_mempool_construct(&this->temp.mp, initial_space);  // Adds 25%
}

template<typename dmtdata_t, typename dmtdataout_t>
void dmt<dmtdata_t, dmtdataout_t>::builder::insert_sorted(const dmtdatain_t &value) {
    paranoid_invariant(this->temp_valid);
    //NOTE: Always use d.a.num_values for size because we have not yet created root.
    if (this->temp.values_same_size && (this->temp.d.a.num_values == 0 || value.get_dmtdatain_t_size() == this->temp.value_length)) {
        this->temp.insert_at_array_end<false>(value);
        return;
    }
    if (this->temp.is_array) {
        // Convert to dtree format (even if ctree exists, it should not be used).
        XMALLOC_N(this->max_values, this->sorted_nodes);

        // Include enough space for alignment padding
        size_t mem_needed = (ALIGNMENT - 1 + __builtin_offsetof(dmt_mnode<true>, value)) * max_values + max_value_bytes;
        struct mempool old_mp = this->temp.mp;

        const uint32_t num_values = this->temp.d.a.num_values;
        toku_mempool_construct(&this->temp.mp, mem_needed);

        // Copy over and get node_idxs
        for (uint32_t i = 0; i < num_values; i++) {
            dmtdatain_t functor(this->temp.value_length, this->temp.get_array_value_internal(&old_mp, i));
            this->sorted_nodes[i] = this->temp.node_malloc_and_set_value<true>(functor);
        }
        this->temp.is_array = false;
        this->temp.values_same_size = false;
        toku_mempool_destroy(&old_mp);
    }
    paranoid_invariant(!this->temp.is_array);
    paranoid_invariant(!this->temp.values_same_size);
    // Insert dynamic.
    this->sorted_nodes[this->temp.d.a.num_values++] = this->temp.node_malloc_and_set_value<true>(value);
}

template<typename dmtdata_t, typename dmtdataout_t>
bool dmt<dmtdata_t, dmtdataout_t>::builder::is_value_length_fixed(void) {
    paranoid_invariant(this->temp_valid);
    return this->temp.values_same_size;
}

template<typename dmtdata_t, typename dmtdataout_t>
void dmt<dmtdata_t, dmtdataout_t>::builder::build_and_destroy(dmt<dmtdata_t, dmtdataout_t> *dest) {
    invariant(this->temp_valid);
    //NOTE: Always use d.a.num_values for size because we have not yet created root.
    invariant(this->temp.d.a.num_values == this->max_values); // Optionally make it <=
    // Memory invariant is taken care of incrementally

    if (!this->temp.is_array) {
        invariant_notnull(this->sorted_nodes);
        this->temp.rebuild_subtree_from_idxs(&this->temp.d.t.root, this->sorted_nodes, this->temp.d.a.num_values);
        toku_free(this->sorted_nodes);
        this->sorted_nodes = nullptr;
    }
    paranoid_invariant_null(this->sorted_nodes);

    size_t used = toku_mempool_get_used_space(&this->temp.mp);
    size_t allocated = toku_mempool_get_size(&this->temp.mp);
    size_t max_allowed = used + used / 4;
    size_t footprint = toku_mempool_footprint(&this->temp.mp);
    if (allocated > max_allowed && footprint > max_allowed) {
        // Reallocate smaller mempool to save memory
        invariant_zero(toku_mempool_get_frag_size(&this->temp.mp));
        struct mempool new_mp;
        toku_mempool_copy_construct(&new_mp, toku_mempool_get_base(&this->temp.mp), toku_mempool_get_used_space(&this->temp.mp));
        toku_mempool_destroy(&this->temp.mp);
        this->temp.mp = new_mp;
    }

    *dest = this->temp;
    this->temp_valid = false;

}
} // namespace toku
