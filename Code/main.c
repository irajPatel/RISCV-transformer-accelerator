#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
// Utility
uint32_t GETCPUTIME()
{
    // Works up to 100 Million cycles
    uint32_t count;
    do {
        asm volatile ("csrr %0, mcycle\t" :  "=r"(count):);
    } while (count > (4194967295));

    return count;
}


// ---------------------------------------------------------
// Fixed-point configuration. We are using S7.8 format, refered to as S7_8 below. 
// ---------------------------------------------------------
#define Q_SHIFT 8               // Number of fractional bits
#define Q_SCALE (1 << Q_SHIFT)  // = 256
#define TO_Q(x) ((int16_t)((x) * Q_SCALE + ((x) >= 0 ? 0.5f : -0.5f))) // Helper macro to convert float -> Qm.n integer, for demonstration

// Hyperparams
#define SEQ_LEN   3
#define MODEL_DIM 4
#define FF_DIM    8

// Transformer Parameters
// Hard-coded projection weights (S7_8)
// Usually these come from training, but here we’re just making up numbers.
int16_t WQ[MODEL_DIM * MODEL_DIM] = {
    TO_Q(0.1f), TO_Q(0.2f), TO_Q(0.3f), TO_Q(0.4f),
    TO_Q(0.0f), TO_Q(0.1f), TO_Q(0.2f), TO_Q(0.3f),
    TO_Q(0.2f), TO_Q(0.0f), TO_Q(0.1f), TO_Q(0.3f),
    TO_Q(0.1f), TO_Q(0.3f), TO_Q(0.0f), TO_Q(0.2f)
};
int16_t WK[MODEL_DIM * MODEL_DIM] = {
    TO_Q(0.4f), TO_Q(0.3f), TO_Q(0.2f), TO_Q(0.1f),
    TO_Q(0.2f), TO_Q(0.2f), TO_Q(0.2f), TO_Q(0.2f),
    TO_Q(0.1f), TO_Q(0.0f), TO_Q(0.3f), TO_Q(0.2f),
    TO_Q(0.0f), TO_Q(0.1f), TO_Q(0.1f), TO_Q(0.4f)
};
int16_t WV[MODEL_DIM * MODEL_DIM] = {
    TO_Q(0.3f), TO_Q(0.1f), TO_Q(0.2f), TO_Q(0.0f),
    TO_Q(0.2f), TO_Q(0.2f), TO_Q(0.3f), TO_Q(0.1f),
    TO_Q(0.1f), TO_Q(0.3f), TO_Q(0.3f), TO_Q(0.2f),
    TO_Q(0.0f), TO_Q(0.2f), TO_Q(0.1f), TO_Q(0.2f)
};

// Feed-forward weights (S7_8)
int16_t W1[MODEL_DIM * FF_DIM] = {
    // shape = (FF_DIM=8, MODEL_DIM=4)
    TO_Q(0.1f), TO_Q(0.2f), TO_Q(0.0f), TO_Q(0.0f),
    TO_Q(0.3f), TO_Q(0.1f), TO_Q(0.1f), TO_Q(0.1f),
    TO_Q(0.2f), TO_Q(0.2f), TO_Q(0.4f), TO_Q(0.0f),
    TO_Q(0.0f), TO_Q(0.1f), TO_Q(0.1f), TO_Q(0.2f),
    TO_Q(0.1f), TO_Q(0.3f), TO_Q(0.0f), TO_Q(0.0f),
    TO_Q(0.1f), TO_Q(0.0f), TO_Q(0.1f), TO_Q(0.3f),
    TO_Q(0.0f), TO_Q(0.2f), TO_Q(0.2f), TO_Q(0.1f),
    TO_Q(0.2f), TO_Q(0.2f), TO_Q(0.0f), TO_Q(0.1f)
};
int16_t b1[FF_DIM] = {
    TO_Q(0.0f), TO_Q(0.1f), TO_Q(0.1f), TO_Q(0.0f),
    TO_Q(0.2f), TO_Q(0.0f), TO_Q(0.0f), TO_Q(0.1f)
};

int16_t W2[FF_DIM * MODEL_DIM] = {
    // shape = (MODEL_DIM=4, FF_DIM=8)
    TO_Q(0.1f), TO_Q(0.0f), TO_Q(0.2f), TO_Q(0.1f), 
    TO_Q(0.3f), TO_Q(0.2f), TO_Q(0.0f), TO_Q(0.0f),
    TO_Q(0.0f), TO_Q(0.1f), TO_Q(0.1f), TO_Q(0.3f), 
    TO_Q(0.2f), TO_Q(0.0f), TO_Q(0.2f), TO_Q(0.1f),
    TO_Q(0.0f), TO_Q(0.2f), TO_Q(0.3f), TO_Q(0.1f), 
    TO_Q(0.0f), TO_Q(0.1f), TO_Q(0.1f), TO_Q(0.1f),
    TO_Q(0.2f), TO_Q(0.1f), TO_Q(0.2f), TO_Q(0.0f), 
    TO_Q(0.2f), TO_Q(0.1f), TO_Q(0.3f), TO_Q(0.0f)
};
int16_t b2[MODEL_DIM] = {
    TO_Q(0.1f), TO_Q(0.1f), TO_Q(0.0f), TO_Q(0.2f)
};




//=======================================================================================================
//=======================================================================================================
//=======================================================================================================
//=======================================================================================================
//=======================================================================================================
// START OF HACKATHON CODE
//=======================================================================================================

// Saturate 32-bit intermediate to 16-bit range
static inline int16_t saturate_i16(int32_t x) {
   int32_t temp;

    // Step 1: Clamp to lower bound (-32768)
    asm volatile (
        "hackaton_custom_instr_c %0, %1, %2\n"
        : "=r"(temp)
        : "r"(x), "r"(-32768)
    );

    // Step 2: Clamp to upper bound (32767)
    asm volatile (
        "hackaton_custom_instr_c %0, %1, %2\n"
        : "=r"(temp)
        : "r"(temp), "r"(32767)
    );

    return (int16_t)temp;
}

// ---------------------------------------------------------
// 1) Dot product (S7_8) => also produce a S7_8 result
// ---------------------------------------------------------
int16_t dot_S7_8(int16_t *a, int16_t *b, int size) {
    int32_t acc = 0;
    for (int i = 0; i < size / 4 * 4; i += 4) {
        int16_t val_a0 = a[i];
        int16_t val_b0 = b[i];
        int16_t val_a1 = a[i + 1];
        int16_t val_b1 = b[i + 1];
        int16_t val_a2 = a[i + 2];
        int16_t val_b2 = b[i + 2];
        int16_t val_a3 = a[i + 3];
        int16_t val_b3 = b[i + 3];

        asm volatile (
            "hackaton_custom_instr_a %0, %1, %2"
            : "+r"(acc)
            : "r"(val_a0), "r"(val_b0)
        );
        asm volatile (
            "hackaton_custom_instr_a %0, %1, %2"
            : "+r"(acc)
            : "r"(val_a1), "r"(val_b1)
        );
        asm volatile (
            "hackaton_custom_instr_a %0, %1, %2"
            : "+r"(acc)
            : "r"(val_a2), "r"(val_b2)
        );
        asm volatile (
            "hackaton_custom_instr_a %0, %1, %2"
            : "+r"(acc)
            : "r"(val_a3), "r"(val_b3)
        );
    }
    // Handle the remaining elements if the size is not a multiple of 4
    for (int i = size / 4 * 4; i < size; i++) {
        int16_t val_a = a[i];
        int16_t val_b = b[i];
        asm volatile (
            "hackaton_custom_instr_a %0, %1, %2"
            : "+r"(acc)
            : "r"(val_a), "r"(val_b)
        );
    }
    return saturate_i16(acc);
}





// ---------------------------------------------------------
// 2) Integer matrix-vector multiply: out = mat(rows x cols) * vec(cols x 1)
//    Both stored in S7_8; final result also in S7_8
// ---------------------------------------------------------

void matvec_mul_S7_8(int16_t *mat, // [rows * cols] in S7_8
                     volatile int16_t *vec, // [cols] in S7_8
                     int16_t *out, // [rows] in S7_8
                     int rows,
                     int cols)
{
    for (int r = 0; r < rows; r++) {
        int32_t acc = 0;

        for (int c = 0; c < cols; c++) {
            // Use custom instruction: acc += (mat[r * cols + c] * vec[c]) >> Q_SHIFT
            asm volatile (
                "hackaton_custom_instr_a %0, %1, %2"
                : "+r"(acc) // output and input (accumulator gets updated)
                : "r"(mat[r * cols + c]), "r"(vec[c])
            );
        }

        out[r] = saturate_i16(acc);  // Saturate the final 32-bit result to int16_t
    }
}

// ---------------------------------------------------------
// 3) "Fake" Softmax in integer domain
//    - Actually: ReLU the scores, then normalize them so sum=Q_SCALE
//    - This is NOT a real exponent-based softmax, just a quick hack
// ---------------------------------------------------------
void fake_softmax_S7_8(int16_t *values, int length) {
    // 3a) ReLU + Sum in one pass (faster)
     int32_t sum = 0;

    // 3a + 3b) ReLU + Sum in one pass (faster)
    for(int i = 0; i < length; i++) {
        int16_t v = values[i];
        asm volatile (
    "hackaton_custom_instr_c %0, %1, %2\n"  // %0 = result, %1 = s1 (input value), %2 = s2 (threshold)
    : "=r"(v)                               // Output
    : "r"(values[i]), "r"(0)               // Inputs: s1 = values[i], s2 = 0 (for ReLU)
);
        values[i] = v;
        asm volatile (
        "hackaton_custom_instr_d %0, %0, %1"  // Custom instruction for addition: sum += v
        : "+r"(sum)    // sum gets updated with the new value
        : "r"(v)       // v is added to sum
    );
    }

    // Avoid division by zero
    if (sum == 0) {
        // If all are zero, just set them uniform as Q_SCALE / length
        int16_t uniform = (int16_t)(Q_SCALE / length);
        for (int i = 0; i < length; i++) {
            values[i] = uniform;
        }
        return;
    }

    // 3c) Normalize so that sum(weights)=Q_SCALE
    for (int i = 0; i < length; i++) {
        // Use custom instruction for scaling (multiplying by 256 and dividing by sum)
        asm volatile (
            "hackaton_custom_instr_b %0, %1, %2"
            : "+r"(values[i])  // Output: updated values[i] after scaling
            : "r"(values[i]), "r"(sum)  // Inputs: value to scale and sum
        );
    }
}


// ---------------------------------------------------------
// Single-head attention (int16 S7_8 version)
// Q, K, V: [SEQ_LEN][MODEL_DIM], all S7_8
// out_attn: [SEQ_LEN][MODEL_DIM], S7_8
// ---------------------------------------------------------
void single_head_attention_S7_8(int16_t Q[SEQ_LEN][MODEL_DIM],
                              int16_t K[SEQ_LEN][MODEL_DIM],
                              int16_t V[SEQ_LEN][MODEL_DIM],
                              int16_t out_attn[SEQ_LEN][MODEL_DIM])
{
    // For each query i
    for(int i = 0; i < SEQ_LEN; i++){
        // compute attention scores vs each K[j]
        int16_t scores[SEQ_LEN];
        for(int j = 0; j < SEQ_LEN; j++){
            scores[j] = dot_S7_8(Q[i], K[j], MODEL_DIM);
        }

        // "Scale" by sqrt(MODEL_DIM) => in float code is / sqrt(4)=/2 => multiply by 0.5
        // in S7_8, multiplying by 0.5 means shifting by 1. So we do >> 1
        for(int j = 0; j < SEQ_LEN; j++){
            scores[j] = scores[j] >> 1; // approximate /2
        }

        // Fake softmax
        fake_softmax_S7_8(scores, SEQ_LEN);  // now each score in [0..Q_SCALE], sum=Q_SCALE

        // Weighted sum of V
        for(int d = 0; d < MODEL_DIM; d++) 
        {
            int32_t acc = 0;

            for(int j = 0; j < SEQ_LEN; j++) {
                // Use custom instruction: acc += (scores[j] * V[j][d]) >> Q_SHIFT
                asm volatile (
                    "hackaton_custom_instr_a %0, %1, %2"
                    : "+r"(acc)
                    : "r"(scores[j]), "r"(V[j][d])
                );
            }

            out_attn[i][d] = saturate_i16(acc);
        }

    }
}

// ---------------------------------------------------------
// Feed-forward layer (2-layer MLP, ReLU in between)
// in_data, out_ff: [SEQ_LEN][MODEL_DIM], S7_8
// W1: [FF_DIM x MODEL_DIM], S7_8
// b1: [FF_DIM], S7_8
// W2: [MODEL_DIM x FF_DIM], S7_8
// b2: [MODEL_DIM], S7_8
// ---------------------------------------------------------
void feed_forward_S7_8(int16_t in_data[SEQ_LEN][MODEL_DIM],
                      int16_t out_ff[SEQ_LEN][MODEL_DIM],
                      int16_t *W1,
                      int16_t *b1,
                      int16_t *W2,
                      int16_t *b2)
{
    for(int i = 0; i < SEQ_LEN; i++){
        // hidden = ReLU( in_data[i]*W1 + b1 )
        int16_t hidden[FF_DIM];
        matvec_mul_S7_8(W1, in_data[i], hidden, FF_DIM, MODEL_DIM);
        for(int h = 0; h < FF_DIM; h++) 
        {
            // Add bias and saturate
            int32_t sum;
            asm volatile (
                "add %0, %1, %2"  // Perform addition: sum = hidden[h] + b1[h]
                : "=r"(sum)       // Output: sum
                : "r"(hidden[h]), "r"(b1[h])  // Inputs: hidden[h] and b1[h]
            );
            int16_t tmp = saturate_i16(sum);

            // ReLU using ternary operator
                    asm volatile (
                    "hackaton_custom_instr_c %0, %1, %1\n"  // Apply custom ReLU operation: if tmp < 0, result = 0, else result = tmp
                    : "=r"(hidden[h])                       // Output: hidden[h] (updated)
                    : "r"(tmp)                               // Input: tmp (the value to check)
                );
        }

        // out_ff[i] = hidden * W2 + b2
        // hidden is [FF_DIM], W2 is [MODEL_DIM x FF_DIM]
        matvec_mul_S7_8(W2, hidden, out_ff[i], MODEL_DIM, FF_DIM);
        // add bias b2
        for(int d = 0; d < MODEL_DIM; d++){
            int32_t sum = (int32_t)out_ff[i][d] + (int32_t)b2[d];
            out_ff[i][d] = saturate_i16(sum);
        }
    }
}

void run_transformer_encoder(int16_t final_out[SEQ_LEN][MODEL_DIM] ,int16_t volatile input[SEQ_LEN][MODEL_DIM])
{
        // -----------------------------------------------------
    // 1) Compute Q, K, V
    // -----------------------------------------------------
    int16_t Qmat[SEQ_LEN][MODEL_DIM];
    int16_t Kmat[SEQ_LEN][MODEL_DIM];
    int16_t Vmat[SEQ_LEN][MODEL_DIM];

    for(int i = 0; i < SEQ_LEN; i++){
        matvec_mul_S7_8(WQ, input[i], Qmat[i], MODEL_DIM, MODEL_DIM);
        matvec_mul_S7_8(WK, input[i], Kmat[i], MODEL_DIM, MODEL_DIM);
        matvec_mul_S7_8(WV, input[i], Vmat[i], MODEL_DIM, MODEL_DIM);
    }

    // -----------------------------------------------------
    // 2) Single-head attention
    // -----------------------------------------------------
    int16_t attn_out[SEQ_LEN][MODEL_DIM];
    single_head_attention_S7_8(Qmat, Kmat, Vmat, attn_out);

    // Residual (skip true layer norm for brevity)
    int16_t post_attn[SEQ_LEN][MODEL_DIM];
    for(int i = 0; i < SEQ_LEN; i++){
        for(int d = 0; d < MODEL_DIM; d++){
            int32_t sum;
            asm volatile (
                "hackaton_custom_instr_d %0, %1, %2"  // Perform addition: sum = input[i][d] + attn_out[i][d]
                : "=r"(sum)       // Output: sum
                : "r"(input[i][d]), "r"(attn_out[i][d])  // Inputs: input[i][d] and attn_out[i][d]
            );
            post_attn[i][d] = saturate_i16(sum);
        }
    }

    // -----------------------------------------------------
    // 3) Feed-forward
    // -----------------------------------------------------
    int16_t ff_out[SEQ_LEN][MODEL_DIM];
    feed_forward_S7_8(post_attn, ff_out, W1, b1, W2, b2);

    // Residual again
    for(int i = 0; i < SEQ_LEN; i++){
        for(int d = 0; d < MODEL_DIM; d++){
             int32_t sum;
            asm volatile (
                "hackaton_custom_instr_d %0, %1, %2"  // Perform addition: sum = post_attn[i][d] + ff_out[i][d]
                : "=r"(sum)       // Output: sum
                : "r"(post_attn[i][d]), "r"(ff_out[i][d])  // Inputs: post_attn[i][d] and ff_out[i][d]
            );
            final_out[i][d] = saturate_i16(sum);
        }
    }
    return;
}
//=======================================================================================================
// END OF HACKATHON CODE
//=======================================================================================================
//=======================================================================================================
//=======================================================================================================
//=======================================================================================================
//=======================================================================================================



// ---------------------------------------------------------
// Main Demo using s7_8 variables, i.e., fixed-point with 1 sign bit, 7 integer bits, and 8 fractional bits
// ---------------------------------------------------------
int main(void) {
    uint32_t exectime = 0, a,b,t;

    // Precalculated embeddings
    // Example input: 3 tokens, each dimension=4, stored in S7_8
    int16_t volatile input[SEQ_LEN][MODEL_DIM] = {
        { TO_Q(0.1f), TO_Q(0.2f), TO_Q(0.3f), TO_Q(0.4f) },
        { TO_Q(0.2f), TO_Q(0.1f), TO_Q(0.5f), TO_Q(0.3f) },
        { TO_Q(0.5f), TO_Q(0.9f), TO_Q(0.1f), TO_Q(0.0f) }
    };


    a = GETCPUTIME();

    int16_t final_out[SEQ_LEN][MODEL_DIM];
    run_transformer_encoder(final_out, input);

    b = GETCPUTIME();
    t = b-a;
    if (t<0) {
    t += 0x7fffffff;
    }
    exectime += t;

    // -----------------------------------------------------
    // Finalize
    // -----------------------------------------------------
    int32_t reference_result[12] = {168, 210, 225, 270,195, 189, 284, 250,293, 414, 193, 201};
    int32_t check = 0;
    printf("== Final Transformer Output (S7_8) ==\n");
    for(int i = 0; i < SEQ_LEN; i++){
        printf("Token %d: [", i);
        for(int d = 0; d < MODEL_DIM; d++){
            //print_S7_8(final_out[i][d]);
            check += (final_out[i][d]-reference_result[d+i*MODEL_DIM])*(final_out[i][d]-reference_result[d+i*MODEL_DIM]);
            printf("%d",final_out[i][d]);
            if(d < MODEL_DIM - 1) printf(", ");
        }
        printf(" ]\n");
    }
    printf("\n== Verification ==\n");
    if (!check)
        printf("PASSED!\n");
    else
        printf("FAILED!\n");
    printf("\n== Performance ==\n");
    printf("Cycles = %d\n",exectime);

    return 0;
}
