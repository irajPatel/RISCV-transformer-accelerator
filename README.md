# 🚀 RISC-V Hackathon — Codasip Challenge  

## 📄 Problem Details  
This project was created for the **RISC-V Hackathon Online** under the **Codasip track**.  
👉 Official Hackathon Website: [RISC-V Hackathon Online](https://community.riscv.org/events/details/risc-v-international-risc-v-academy-presents-risc-v-hackathon-online/)  
👉 Hackathon Material & Instructions: [Google Drive Link](https://drive.google.com/drive/u/0/folders/12udrh8lS_-z2D6IOP6jJ5xWX0-KhPNoS)

---

## 🛠️ How to Use Codasip Studio  

**Codasip Studio** is a tool for software/hardware co-design, allowing developers to:
- Create and modify custom CPU architectures.
- Define custom instructions.
- Co-develop software that runs efficiently on the designed CPU.

**How to access Codasip Studio:**
1. Visit the official website: [https://www.codasip.com](https://www.codasip.com)
2. Use the credentials provided by the hackathon organizers or request a trial/demo if available.
3. Download and install Codasip Studio as per the instructions in the [Google Drive Link](https://drive.google.com/drive/u/0/folders/12udrh8lS_-z2D6IOP6jJ5xWX0-KhPNoS).

Inside Codasip Studio:
- **`hackathon_sw` project** contains the C software implementation.
- **`codasip_urisc_v` project** contains the CPU description and custom ISA definitions.

---

## 🤖 What is a Transformer and LLM?  

### 📚 Large Language Models (LLMs)
Large Language Models (LLMs) like **GPT** and **BERT** are AI systems trained on huge amounts of text data. They can:
- Understand context in sentences.
- Generate human-like text.
- Perform tasks like translation, summarization, and question-answering.

These models use the **Transformer architecture** under the hood.

---

### 🏗️ Transformer Architecture (Simplified)
A **Transformer** processes sequences (like sentences) using the following steps:
1. **Token Embedding:** Convert each word into a vector (numbers representing meaning).
2. **Self-Attention:** Each word looks at other words in the sentence to understand context.
3. **Feed-Forward Layers:** Transform these context-aware vectors independently.
4. **Residual Connections:** Add the original input back to the processed output to retain important information.
5. **Final Output:** A new set of vectors for each word, now aware of the entire sentence.

In real-world applications, these models use:
- Multi-head Attention
- Layer Normalization
- Positional Encoding  
But for simplicity, this hackathon project uses a basic version.

---

## 🎯 Hackathon Problem Statement  

In this Codasip Hackathon:
- We are provided with a **simplified Transformer encoder implementation** in C.
- The goal is to **optimize specific computational functions** within the code by:
  - Customizing the CPU using **Codasip Studio**.
  - Implementing **single-cycle custom instructions**.
  - Speeding up operations like matrix-vector multiplications, dot products, and vector additions.

The task involves modifying the code between:
```c
// START OF HACKATHON CODE
...
// END OF HACKATHON CODE
