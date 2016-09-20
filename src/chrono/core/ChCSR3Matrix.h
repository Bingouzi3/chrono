// =============================================================================
// PROJECT CHRONO - http://projectchrono.org
//
// Copyright (c) 2014 projectchrono.org
// All right reserved.
//
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file at the top level of the distribution and at
// http://projectchrono.org/license-chrono.txt.
//
// =============================================================================
// Authors: Dario Mangoni, Radu Serban
// =============================================================================

#ifndef CHCSR3MATRIX_H
#define CHCSR3MATRIX_H

#define ALIGNED_ALLOCATORS

#include <limits>

#include "chrono/core/ChSparseMatrix.h"
#include "chrono/core/ChAlignedAllocator.h"
#include "ChTimer.h"

namespace chrono {
	class ChSparsityPatternLearner;
	class ChMapMatrix;

	/// @addtogroup chrono
/// @{

/* ChCSR3Matrix is a class that implements CSR3 sparse matrix format;
* - The more useful constructor specifies rows, columns and nonzeros
* - The argument "nonzeros": if 0<nonzeros<=1 specifies non-zeros/(rows*columns);
*                            if nonzeros>1 specifies exactly the number non-zeros in the matrix.
* - It's better to overestimate the number of non-zero elements to avoid reallocations in memory.
* - Each of the 3 arrays is stored contiguously in memory (e.g. as needed by MKL Pardiso).
* - The array of column indexes (colIndex) is initialized with "-1": that means that the corrisponing element in the
"values" array
*   doesn't hold any significant number, so it can be overwritten.
* - It's preferrable to insert elements in the matrix in increasing column order to avoid rearranging.
* - When a new element should be inserted the algorithm seeks the nearest not-initialized location (i.e. with "-1" in
colIndex);
    if it has to search too far ("max_shifts" exceeded) or if it finds no available spaces THEN it reallocates the
arrays
* It's better to use GetElement to read from matrix; Element() creates the space if the element does not exist.
*/

// The CSR3 format for a 3x3 matrix is like this:
//  | 1.1  1.2  1.3 |    values =   { 1.1, 1.2, 1.3, 2.2, 2.3, 3.3 };
//  |  0   2.2  2.3 |	 colIndex = {  0,   1,   2,   1,   2,   2  };
//  |  0    0   3.3 |	 rowIndex = {  0,             3,        5  , 6};
// but it's difficult to have an exact estimate of how many nonzero element there will be before actually storing them;
// so how many location should be preallocated? an overestimation is usually preferred to avoid further reallocations.
// Let's say that we would like to allocate all the 9 elements: (NI means Not Initialized)
//  | 1.1  1.2  1.3 |    values =   { 1.1, 1.2, 1.3, 2.2, 2.3, NI, 3.3, NI, NI };
//  |  0   2.2  2.3 |	 colIndex = {  0,   1,   2,   1,   2,  -1,  2,  -1, -1 };
//  |  0    0   3.3 |	 rowIndex = {  0,             3,            6,          , 9 };
// So, if a new element should be stored (e.g. the [2,0] element) only one insignificant arrangement should be done
// instead of reallocating the arrays:
// the algorithm, starting from colIndex[6] will find the nearest uninitialized space (i.e. a colIndex cell that has
// "-1" in it) and moves the elements
// in order to let the new element to be written in that place!
// When all the writing operations are performed the matrix can be "compressed" (i.e. call Compress()): all the
// uninitialized locations are purged.

/*
* Reset VS Resize
* Reset() function initializes arrays to their default values. Always succesfull.
* Resize() always preserve data in the arrays. The return value tells the user if the resizing has been done.
*
* Reset() and Resize() eventually expands the arrays dimension (increase occupancy)
* but they DO NOT REDUCE the occupancy. Eventually it has to be done manually with Trim().
*/
class ChApi ChCSR3Matrix : public ChSparseMatrix {
  private:
    const bool row_major_format = true;
    const static int array_alignment = 64;
    bool isCompressed = false;
    int max_shifts = std::numeric_limits<int>::max();

    // CSR matrix arrays.
#ifdef ALIGNED_ALLOCATORS
	typedef std::vector<int, aligned_allocator<int, array_alignment>> index_vector_t;
	typedef std::vector<double, aligned_allocator<double, array_alignment>> values_vector_t;
#else
	typedef std::vector<int> index_vector_t;
	typedef std::vector<double> values_vector_t;
#endif

	index_vector_t leadIndex;
	index_vector_t trailIndex;
	values_vector_t values;
	std::vector<bool> initialized_element;
    int* leading_dimension = nullptr;
    int* trailing_dimension = nullptr;

    bool m_lock_broken = false;  ///< true if a modification was made that overrules m_lock

  protected:
	void static distribute_integer_range_on_vector(index_vector_t& vector, int initial_number, int final_number);
	void reset_arrays(int lead_dim, int trail_dim, int nonzeros);
	void insert(int& trail_sel, const int& lead_sel);
	void copy_and_distribute(const index_vector_t& trailIndex_src,
							 const values_vector_t& values_src,
							 const std::vector<bool>& initialized_element_src,
							 index_vector_t& trailIndex_dest,
							 values_vector_t& values_dest,
							 std::vector<bool>& initialized_element_dest,
							 int& trail_ins, int lead_ins,
							 int storage_augm);

    static void resize_to_their_limits(index_vector_t& trailIndex_in,
                                       values_vector_t& values_in,
                                       std::vector<bool>& initialized_element_in,
                                       int new_size);

  public:
    ChCSR3Matrix(int nrows = 1, int ncols = 1, bool row_major_format_on = true, int nonzeros = 1);
    virtual ~ChCSR3Matrix(){};

    virtual void SetElement(int row_sel, int col_sel, double insval, bool overwrite = true) override;
    virtual double GetElement(int row_sel, int col_sel) const override;

    double& Element(int row_sel, int col_sel);
    double& operator()(int row_sel, int col_sel) { return Element(row_sel, col_sel); }
    double& operator()(int index) { return Element(index / m_num_cols, index % m_num_cols); }

    // Size manipulation
    virtual void Reset(int nrows, int ncols, int nonzeros = 0) override;
    virtual bool Resize(int nrows, int ncols, int nonzeros = 0) override {
        Reset(nrows, ncols, nonzeros);
        return true;
    }

    /// Get the number of non-zero elements in this matrix.
    virtual int GetNNZ() const override { return trailIndex.size(); }

    /// Return the row index array in the CSR representation of this matrix.
	virtual int* GetCSR_LeadingIndexArray() const override;

    /// Return the column index array in the CSR representation of this matrix.
	virtual int* GetCSR_TrailingIndexArray() const override;

    /// Return the array of matrix values in the CSR representation of this matrix.
	virtual double* GetCSR_ValueArray() const override;

    /// Compress the internal arrays and purge all uninitialized elements.
    virtual bool Compress() override;

    /// Trims the internal arrays to have exactly the dimension needed, nothing more.
    /// Data arrays are not moved.
    void Trim();
    void Prune(double pruning_threshold = 0);

    // Auxiliary functions
	int GetTrailingIndexLength() const { return leadIndex[*leading_dimension]; }
    int GetTrailingIndexCapacity() const { return trailIndex.capacity(); }

    void SetMaxShifts(int max_shifts_new = std::numeric_limits<int>::max()) { max_shifts = max_shifts_new; }
    bool IsCompressed() const { return isCompressed; }

    // Testing functions
    int VerifyMatrix() const;

    // Import/Export functions
	void ImportFromDatFile(std::string filepath = "", bool row_major_format_on = true);
    void ExportToDatFile(std::string filepath = "", int precision = 6) const;
	void LoadFromMapMatrix(ChMapMatrix& map_mat);
	void LoadSparsityPattern(ChSparsityPatternLearner& sparsity_dummy);


	ChTimer<> timer_insert;
	ChTimer<> timer_reset;
	ChTimer<> timer_setelement;

	int counter_insert = 0;
	int counter_reset = 0;
	int counter_setelement = 0;

};

class ChApi ChSparsityPatternLearner : public ChSparseMatrix
{
protected:
	std::vector<std::list<int>> row_lists;
	bool row_major_format = true;
	int* leading_dimension;
	int* trailing_dimension;

public:
	ChSparsityPatternLearner(int nrows, int ncols, bool row_major_format_in) :
		ChSparseMatrix(nrows, ncols)
	{
		row_major_format = row_major_format_in;
		leading_dimension = row_major_format ? &m_num_rows : &m_num_cols;
		trailing_dimension = row_major_format ? &m_num_cols : &m_num_rows;
		row_lists.resize(*leading_dimension);
	}

	virtual ~ChSparsityPatternLearner() {}

	void SetElement(int insrow, int inscol, double insval, bool overwrite = true) override
	{
		row_lists[insrow].push_back(inscol);
	}

	double GetElement(int row, int col) const override { return 0.0; }

	void Reset(int row, int col, int nonzeros = 0) override
	{
		*leading_dimension = row_major_format ? row : col;
		*trailing_dimension = row_major_format ? col : row;
		row_lists.clear();
		row_lists.resize(*leading_dimension);
	}

	bool Resize(int nrows, int ncols, int nonzeros = 0) override
	{
		Reset(nrows, ncols, nonzeros);
		return true;
	}

	std::vector<std::list<int>>& GetSparsityPattern()
	{
		for (auto& list : row_lists)
		{
			list.sort();
			list.unique();
		}
		return row_lists;
	}

	bool isRowMajor() const { return row_major_format; }

	int GetNNZ() const override {
		int nnz_temp = 0;
		for each (auto list in row_lists)
			nnz_temp += list.size();

		const_cast<ChSparsityPatternLearner*>(this)->m_nnz = nnz_temp;
		return nnz_temp;
	}
};

	/// @} chrono

};  // end namespace chrono

#endif
