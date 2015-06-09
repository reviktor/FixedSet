#include <iostream>
#include <random>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <limits>

int64_t mod(int64_t dividend, int64_t divisor) {
    int64_t result = dividend % divisor;
    if (result < 0) {
        return result + divisor;
    }
    return result;
}


class Hash {
public:
    Hash() {
    }

    explicit Hash(int64_t factor, int64_t addend, size_t tableSize,
            int64_t prime = DEFAULT_PRIME_MODULUS) :
            factor(factor), addend(addend), prime(prime), tableSize(tableSize) {
    }

    size_t operator()(int64_t key) const {
        return mod(mod(factor * key + addend, prime), tableSize);
    }

    static const int64_t DEFAULT_PRIME_MODULUS = 2147483647;

private:
    int64_t factor;
    int64_t addend;
    int64_t prime;
    int64_t tableSize;
};

template<typename Generator>
Hash generateHashFunction(size_t tableSize, Generator& generator) {
    std::uniform_int_distribution<int> factorDistribution =
            std::uniform_int_distribution<int>(1, Hash::DEFAULT_PRIME_MODULUS - 1);
    std::uniform_int_distribution<int> addendDistribution =
            std::uniform_int_distribution<int>(0, Hash::DEFAULT_PRIME_MODULUS - 1);

    return Hash(factorDistribution(generator), addendDistribution(generator),
            tableSize);
}


std::vector<unsigned int> computeDistribution(const std::vector<int>& numbers,
        const size_t tableSize, const Hash& hash) {
    std::vector<unsigned int> distribution(tableSize, 0);
    for (const auto& key : numbers) {
        ++distribution[hash(key)];
    }
    return distribution;
}

std::vector<std::vector<int>> distributeNumbers(const std::vector<int>& numbers,
        const size_t tableSize, const Hash& hash) {
    std::vector<std::vector<int>> chains(tableSize);
    for (const auto& key : numbers) {
        chains[hash(key)].push_back(key);
    }
    return chains;
}

std::vector<int> flattenDistribution(const std::vector<std::vector<int>>& distribution,
        const int NO_ELEMENT) {
    std::vector<int> data(distribution.size());
    for (size_t i = 0; i < distribution.size(); ++i) {
        if (!distribution[i].empty()) {
            if (distribution[i].size() > 1) {
                throw std::invalid_argument("inner vector contains more than 1 element");
            }
            data[i] = distribution[i][0];
        } else {
            data[i] = NO_ELEMENT;
        }
    }
    return data;
}

template<typename T>
class MultiplyUnsignedThrowingOverflowException {
public:
    T operator()(const T& first, const T& second) {
        T product = first * second;
        if (second != 0 && product / first != second) {
            throw std::overflow_error("");
        }
        return product;
    }
};

size_t square(size_t size) {
    MultiplyUnsignedThrowingOverflowException<size_t>  multiplier;
    return multiplier(size, size);
}


class SecondLevelTable {
public:
    SecondLevelTable() {
    }

    template<typename Generator>
    void initialize(const std::vector<int>& numbers, Generator& generator) {
        if (numbers.empty()) {
            return;
        }

        size_t tableSize = square(numbers.size());
        bool withoutCollisions;
        do {
            hash = generateHashFunction(tableSize, generator);

            std::vector<unsigned int> distribution = computeDistribution(numbers,
                    tableSize, hash);

            withoutCollisions = !std::any_of(distribution.begin(), distribution.end(),
                    [](unsigned int count) {return count > 1;
                    });

            if (withoutCollisions) {
                auto distributedNumbers = distributeNumbers(numbers, tableSize, hash);
                data = flattenDistribution(distributedNumbers, NO_ELEMENT);
            }
        } while (!withoutCollisions);
    }

    bool contains(int number) const {
        if (data.empty()) {
            return false;
        }

        return data[hash(number)] == number;
    }

    static const int NO_ELEMENT = 1000000001;

private:
    std::vector<int> data;
    Hash hash;
};


template<typename T>
class AddUnsignedThrowingOverflowException {
public:
    T operator()(const T& first, const T& second) {
        if (first > (std::numeric_limits<T>::max() - second)) {
            throw std::overflow_error("");
        }
        return first + second;
    }
};


class FixedSet {
public:
    FixedSet() {
        std::random_device randomDevice;
        generator = std::default_random_engine(randomDevice());
    }

    void initialize(const std::vector<int>& numbers) {
        size_t numbersCount = numbers.size();
        std::vector<std::vector<int>> chains;
        bool inequalityHolds = false;

        do {
            firstLevelHash = generateHashFunction(numbersCount, generator);

            std::vector<unsigned int> distribution = computeDistribution(numbers, numbersCount,
                    firstLevelHash);

            const uint64_t initialValue = 0;
            uint64_t sumOfCountSquares = std::inner_product(distribution.begin(),
                    distribution.end(), distribution.begin(), initialValue,
                    AddUnsignedThrowingOverflowException<uint64_t>(),
                    MultiplyUnsignedThrowingOverflowException<uint64_t>());
            if (sumOfCountSquares <= UPPER_BOUND_MULTIPLIER * numbersCount) {
                inequalityHolds = true;
                chains = distributeNumbers(numbers, numbersCount, firstLevelHash);
            }
        } while (!inequalityHolds);

        secondLevelTables.resize(numbersCount);
        for (size_t counter = 0; counter < numbersCount; ++counter) {
            secondLevelTables[counter].initialize(chains[counter], generator);
        }
    }

    bool contains(int number) const {
        size_t firstLevelIndex = firstLevelHash(number);
        return secondLevelTables[firstLevelIndex].contains(number);
    }

    static const size_t UPPER_BOUND_MULTIPLIER = 3;

private:
    std::default_random_engine generator;
    Hash firstLevelHash;
    std::vector<SecondLevelTable> secondLevelTables;
};


std::vector<bool> processQueries(const std::vector<int>& numbers,
        const std::vector<int>& queries) {
    FixedSet fixedSet;
    std::vector<bool> answers;
    answers.reserve(queries.size());

    fixedSet.initialize(numbers);
    for (const auto& query : queries) {
        answers.push_back(fixedSet.contains(query));
    }

    return answers;
}

std::vector<int> readNumbers(std::istream& inputStream = std::cin) {
    size_t numbersCount;
    inputStream >> numbersCount;
    std::vector<int> numbers(numbersCount);

    for (auto& number : numbers) {
        inputStream >> number;
    }

    return numbers;
}

std::vector<int> readQueries(std::istream& inputStream = std::cin) {
    size_t queriesCount;
    std::cin >> queriesCount;
    std::vector<int> queries(queriesCount);

    for (auto& query : queries) {
        std::cin >> query;
    }

    return queries;
}

void printAnswers(const std::vector<bool>& answers,
        std::ostream& outputStream = std::cout) {
    for (const auto& answer : answers) {
        if (answer) {
            outputStream << "Yes\n";
        } else {
            outputStream << "No\n";
        }
    }
}


int main() {
    const std::vector<int> numbers = readNumbers();
    const std::vector<int> queries = readQueries();

    std::vector<bool> answers = processQueries(numbers, queries);

    printAnswers(answers);

    return 0;
}
