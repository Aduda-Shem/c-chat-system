import random

word_list = [
    "apple", "banana", "cherry", "date", "elderberry", "fig", "grape", "honeydew",
    "kiwi", "lemon", "mango", "nectarine", "orange", "pear", "quince", "raspberry",
    "strawberry", "tangerine", "uva", "watermelon"
]

dataset_size = 10  
output_file = "intext1.txt"

random_data = [random.choice(word_list) for _ in range(dataset_size)]

with open(output_file, 'w') as file:
    file.write("\n".join(random_data))

print(f"Generated a dataset of {dataset_size} words and saved to {output_file}")
