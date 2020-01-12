/*
Copyright <2019> <Pedro Faustini>

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "../include/tfidf_vectorizer.h"


TfIdfVectorizer::TfIdfVectorizer(bool binary, bool lowercase, bool use_idf, int max_features, std::string norm, bool sublinear_tf)
{
    this->binary = binary;
    this->max_features = max_features; // -1 uses all words.
    if (norm == "l2") this->p = 2;
    else if (norm == "l1") this->p = 1;
    else this->p = 0;
    this->lowercase = lowercase;
    this->use_idf = use_idf;
    this->sublinear_tf = sublinear_tf;
}


std::vector<std::string> TfIdfVectorizer::tokenise_document(std::string& document)
{
    std::vector<std::string> tokens;
    std::string s;

    boost::tokenizer<> tok(document);
    for(boost::tokenizer<>::iterator beg=tok.begin(); beg!=tok.end();++beg)
    {
        s = *beg;
        if (this->lowercase)
            boost::algorithm::to_lower(s);
        tokens.push_back(s);
    }
    return tokens;
}


std::vector<std::vector<std::string>> TfIdfVectorizer::tokenise_documents(std::vector<std::string>& documents)
{
    std::vector<std::vector<std::string>> documents_tokenised;
    for (size_t i = 0; i < documents.size(); i++)
        documents_tokenised.push_back(tokenise_document(documents[i]));
    return documents_tokenised;
}

std::vector<std::map<std::string, int>> TfIdfVectorizer::word_count(std::vector<std::vector<std::string>>& documents_tokenised)
{
    std::vector<std::map<std::string, int>> documents_word_counts;
    std::string word;
    std::set<std::string> words_set;
    for (size_t d = 0; d < documents_tokenised.size(); d++) // d: document index.
    {
        std::map<std::string, int> wc;
        documents_word_counts.push_back(wc);
        for (size_t w = 0; w < documents_tokenised[d].size(); w++) // w: word index.
        {
            word = documents_tokenised[d][w];
            documents_word_counts[d][word] += 1;
            words_set.insert(word);
        }
    }

    size_t i = 0;
    for (std::set<std::string>::iterator it = words_set.begin(); it != words_set.end(); ++it)
    {
        word = *it;
        this->vocabulary_[word] = i;
        i++;
    }
    
    return documents_word_counts;
}

void TfIdfVectorizer::fit(std::vector<std::string>& documents)
{
    this->vocabulary_.clear();
    this->idf_.clear();
    std::vector<std::vector<std::string>> documents_tokenised = tokenise_documents(documents);
    std::vector<std::map<std::string, int>> documents_word_counts = word_count(documents_tokenised);
    idf(documents_word_counts);
}

std::map<std::string, double> TfIdfVectorizer::idf(std::vector<std::map<std::string, int>>& documents_word_counts)
{
    std::string key;
    int value;
    size_t i;
    size_t documents = documents_word_counts.size();
    double d_documents = (double)documents;
    double temp_idf;
    for (auto it = this->vocabulary_.cbegin(); it != this->vocabulary_.cend(); ++it)
    {
        key = (*it).first;
        value = 0;
        for (i = 0; i < documents; i++)
        {
            if (documents_word_counts[i][key] > 0)
                value++;
        }
        /*Adding both denominator and numerator by 1 to avoid division by 0 AND negative idf */
        temp_idf = std::log((d_documents + 1) / (value + 1)) + 1; //log+1 avoids terms with zero idf to be suppressed.
        this->idf_[key] = temp_idf;
    }

    /*Get only the words with highest idf.*/
    if (this->max_features > 0)
    {
        /*Ordered set, reversed order by value*/
        std::set<std::pair<int, std::string>> temp_idf;
        for (auto const& x : this->idf_)
        {
            std::pair<int, std::string> elem (x.second, x.first);
            temp_idf.insert(elem);
        }

        /*Get the most important words from the ordered set */
        this->idf_.clear();
        this->vocabulary_.clear();
        int i = 0;
        std::set<std::pair<int, std::string>>::reverse_iterator rit;
        for (rit = temp_idf.rbegin(); rit != temp_idf.rend(); rit++)
        {
            if (i == this->max_features)
                break; 
            this->idf_[rit->second] = rit->first;
            this->vocabulary_[rit->second] = i;
            i++;
        }
    }
    return this->idf_;
}

std::vector<std::map<std::string, double>> TfIdfVectorizer::tf(std::vector<std::vector<std::string>>& documents_tokenised)
{
    std::vector<std::map<std::string, double>> documents_word_frequency;
    std::string word;
    for (size_t d = 0; d < documents_tokenised.size(); d++) // d: document index.
    {
        std::map<std::string, double> wf;
        documents_word_frequency.push_back(wf);
        for (size_t w = 0; w < documents_tokenised[d].size(); w++) // w: word index.
        {
            word = documents_tokenised[d][w];
            documents_word_frequency[d][word] += 1;
        }
        for (auto& s : documents_word_frequency[d])
        {
            if(this->binary) // If True, all non-zero term counts are set to 1. 
                documents_word_frequency[d][s.first] = (documents_word_frequency[d][s.first] > 0) ? 1 : 0;
            else // Computes tf dividing term count by doc size.
            {
                documents_word_frequency[d][s.first] /= documents_tokenised[d].size();
                if(this->sublinear_tf)
                    documents_word_frequency[d][s.first] = 1 + std::log(documents_word_frequency[d][s.first]);
            }
        } 
    }
    return documents_word_frequency;
}

arma::mat TfIdfVectorizer::fit_transform(std::vector<std::string>& documents)
{
    fit(documents);
    return transform(documents);
}

arma::mat TfIdfVectorizer::transform(std::vector<std::string>& documents)
{
    std::vector<std::vector<std::string>> documents_tokenised = tokenise_documents(documents);
    std::vector<std::map<std::string, double>> documents_word_counts = tf(documents_tokenised);
    arma::mat X_transformed(this->vocabulary_.size(), documents.size());

    std::map<std::string, double> tf_hash;
    std::string word;
    size_t w;
    double idf;
    for (size_t d = 0; d < documents.size(); d++)
    {
        tf_hash = documents_word_counts[d];
        for (auto& s : this->idf_)
        {
            word = s.first;
            w = this->vocabulary_[word];
            idf = s.second;
            if (this->use_idf)
                X_transformed(w, d) = tf_hash[word] * idf;
            else
                X_transformed(w, d) = (tf_hash[word] > 0) ? 1 : 0;
        }
    }

    /*Normalize vectors.*/
    if (this->p != 0)
    {
        for (size_t c = 0; c < X_transformed.n_cols; c++)
        {
            double norm = 0;
            for (size_t r = 0; r < X_transformed.n_rows; r++)
                norm += std::pow(X_transformed(r, c), this->p);
            norm = std::sqrt(norm);
            if(norm > 0)
                for (size_t r = 0; r < X_transformed.n_rows; r++)
                    X_transformed(r, c) /= norm;
        }
    }
    return X_transformed;
}


std::map<std::string, double> TfIdfVectorizer::get_idf_()
{
    const std::map<std::string, double> i = this->idf_;
    return i;
}

std::map<std::string, size_t> TfIdfVectorizer::get_vocabulary_()
{
    const std::map<std::string, size_t> v = this->vocabulary_;
    return v;
}