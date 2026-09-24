#!/bin/bash

count=$#
sum=0

for num in "$@"; do
    sum=$((sum + num))
done

if [ "$count" -eq 0 ]; then
    echo "Нет аргументов"
    exit 1
fi

average=$((sum / count))
echo "Количество: $count"
echo "Среднее арифметическое: $average"
