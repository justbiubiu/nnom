/*
 * Copyright (c) 2018-2019
 * Jianjia Ma, Wearable Bio-Robotics Group (WBR)
 * majianjia@live.com
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2019-02-05     Jianjia Ma   The first version
 * 2019-02-14	  Jianjia Ma   Add layer.free() method.
 */

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include "nnom.h"
#include "nnom_tensor.h"

 // tensor size
size_t tensor_size(nnom_tensor_t* t)
{
	size_t size = 0;
	if (t != NULL)
	{
		size = t->dim[0];
		for (int i = 1; i < t->num_dim; i++)
			size *= t->dim[i];
	}
	return size;
}


size_t tensor_get_num_channel(nnom_tensor_t* t)
{
	// this will need to be changed to support batch. 
#ifdef NNOM_USING_CHW
	// channel first
	//return t->dim[0];
	return t->dim[t->num_dim -1];		// we are always using hwc to describe even our data is in CHW
#else
	// channel last
	return t->dim[t->num_dim -1];
#endif
}

// initialise/create new tensor
nnom_tensor_t* new_tensor(nnom_qtype_t type, uint32_t num_dim, uint32_t num_channel)
{
	nnom_tensor_t* t = NULL;
	if(type == NNOM_QTYPE_PER_AXIS)
	{
		t = nnom_mem(nnom_alignto(sizeof(nnom_tensor_t), 4) 
								+ num_dim*sizeof(nnom_shape_data_t) 
								+ num_channel*sizeof(nnom_qformat_param_t)*2);
		if(t == NULL)
			return t;
		t->dim = (nnom_shape_data_t*)((uint8_t*)t + sizeof(nnom_tensor_t));	// should add alignment
		t->q_dec = (nnom_qformat_param_t*)((uint8_t*)t->dim + num_dim*sizeof(nnom_shape_data_t));
		t->q_offset = (nnom_qformat_param_t*)((uint8_t*)t->q_dec + num_channel*sizeof(nnom_qformat_param_t));
		t->num_dim = num_dim;
		t->qtype = type;
	}
	else if (type == NNOM_QTYPE_PER_TENSOR)
	{
		t = nnom_mem(nnom_alignto(sizeof(nnom_tensor_t), 4) 
								+ num_dim*sizeof(nnom_shape_data_t) 
								+ 2*sizeof(nnom_qformat_param_t));
		if(t == NULL)
			return t;
		t->dim = (nnom_shape_data_t*)((uint8_t*)t + sizeof(nnom_tensor_t));
		t->q_dec = (nnom_qformat_param_t*)((uint8_t*)t->dim + num_dim*sizeof(nnom_shape_data_t));
		t->q_offset = (nnom_qformat_param_t*)((uint8_t*)t->q_dec + sizeof(nnom_qformat_param_t));
		t->num_dim = num_dim;
		t->qtype = type;
	}
	else
	{
		NNOM_LOG("ERROR: tensor type not specified\n");
	}
	return t;
}

void delete_tensor(nnom_tensor_t* t)
{
	if (t)
		nnom_free(t);
}

// set tensor by value
// for tensor with quantized type NNOM_QTYPE_PER_TENSOR
nnom_tensor_t* tensor_set_attr_v(nnom_tensor_t* t, 
		nnom_qformat_param_t dec_bit, nnom_qformat_param_t offset, nnom_shape_data_t* dim, uint32_t num_dim, uint8_t bitwidth)
{
	// copy dim
	t->num_dim = num_dim;
	memcpy(t->dim, dim, sizeof(nnom_shape_data_t) * num_dim);

	// bitwidth
	t->bitwidth = bitwidth;
	// copy the offset and q format
	*(t->q_dec) = dec_bit;
	*(t->q_offset) = offset;
	return t;
}


// set tensor by pointer
// for tensor with quantized type NNOM_QTYPE_PER_AXIS
nnom_tensor_t* tensor_set_attr(nnom_tensor_t* t, 
		nnom_qformat_param_t*dec_bit, nnom_qformat_param_t *offset, nnom_shape_data_t* dim, uint32_t num_dim, uint8_t bitwidth)
{
	// copy dim
	t->num_dim = num_dim;
	memcpy(t->dim, dim, sizeof(nnom_shape_data_t) * num_dim);

	// bitwidth
	t->bitwidth = bitwidth;
	// copy the offset and q format
	memcpy(t->q_dec, dec_bit, sizeof(nnom_qformat_param_t)*tensor_get_num_channel(t));
	memcpy(t->q_offset, offset, sizeof(nnom_qformat_param_t)*tensor_get_num_channel(t));
	return t;
}

// this method copy the attributes of a tensor to a new tensor
// Note, the tensors must have the same lenght. this method wont cpy the memory pointer data (we will assign memory later after building)
nnom_tensor_t* tensor_cpy_attr(nnom_tensor_t* des, nnom_tensor_t* src)
{
	des->bitwidth = src->bitwidth;
	// copy number the qtype
	des->qtype = src->qtype;
	memcpy(des->q_dec, src->q_dec, sizeof(nnom_qformat_param_t)*tensor_get_num_channel(src));
	memcpy(des->q_offset, src->q_offset, sizeof(nnom_qformat_param_t)*tensor_get_num_channel(src));

	// copy number of dimension
	des->num_dim = src->num_dim;
	memcpy(des->dim, src->dim, src->num_dim * sizeof(nnom_shape_data_t));
	return des;
}

// change format from CHW to HWC
// the shape of the data, input data, output data
void tensor_hwc2chw_q7(nnom_tensor_t* des, nnom_tensor_t* src)
{
	q7_t* p_out = des->p_data;
	q7_t* p_in = src->p_data;

	for (int c = 0; c < src->dim[2]; c++)
	{
		for (int h = 0; h < src->dim[0]; h++)
		{
			for (int w = 0; w < src->dim[1]; w++)
			{
				*p_out = p_in[(h * src->dim[1] + w) * src->dim[2] + c];
				p_out++;
			}
		}
	}
}


// only support 3d tensor
// change format from CHW to HWC
// the shape of the data, input data, output data
void tensor_chw2hwc_q7(nnom_tensor_t* des, nnom_tensor_t* src)
{
	q7_t* p_out = des->p_data;
	q7_t* p_in = src->p_data;

	int im_size = 1;
	int h_step;
	h_step = src->dim[0];

	for (int i = 1; i < src->num_dim; i++)
		im_size *= src->dim[i];

	for (int h = 0; h < src->dim[0]; h++)
	{
		h_step = src->dim[1] * h;
		for (int w = 0; w < src->dim[1]; w++)
		{
			// for 3d tensor
			for (int c = 0; c < src->dim[1]; c++)
			{
				*p_out = p_in[im_size * c + h_step + w];
				p_out++;
			}
		}
	}

}

// (deprecated by tensor_hwc2chw version)
// change format from CHW to HWC
// the shape of the data, input data, output data
void hwc2chw_q7(nnom_shape_t shape, q7_t* p_in, q7_t* p_out)
{
	for (int c = 0; c < shape.c; c++)
	{
		for (int h = 0; h < shape.h; h++)
		{
			for (int w = 0; w < shape.w; w++)
			{
				*p_out = p_in[(h * shape.w + w) * shape.c + c];
				p_out++;
			}
		}
	}
}

// (deprecated)
// change format from CHW to HWC
// the shape of the data, input data, output data
void chw2hwc_q7(nnom_shape_t shape, q7_t* p_in, q7_t* p_out)
{
	int im_size = shape.w * shape.h;
	int h_step;

	for (int h = 0; h < shape.h; h++)
	{
		h_step = shape.w * h;
		for (int w = 0; w < shape.w; w++)
		{
			for (int c = 0; c < shape.c; c++)
			{
				*p_out = p_in[im_size * c + h_step + w];
				p_out++;
			}
		}
	}
}