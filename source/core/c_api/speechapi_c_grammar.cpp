//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// speechapi_c_grammar.cpp: Public API definitions for Grammar related C methods
//

#include "stdafx.h"
#include "create_object_helpers.h"
#include "handle_helpers.h"
#include "resource_manager.h"


using namespace Microsoft::CognitiveServices::Speech::Impl;

SPXHR phrase_list_grammar_from_recognizer_by_name_impl(CSpxHandleTable<ISpxRecognizer, SPXRECOHANDLE>*, SPXRECOHANDLE, std::shared_ptr<ISpxRecognizer>, std::shared_ptr<ISpxGrammarList> grammarlist, const char* name, SPXGRAMMARHANDLE* hgrammar)
{
    auto grammar = grammarlist->GetPhraseListGrammar(PAL::ToWString(name).c_str());
    
    auto ok = grammar != nullptr;
    if (ok) *hgrammar = CSpxApiManager::TrackHandle<ISpxGrammar, SPXGRAMMARHANDLE>(grammar);
    
    return ok ? SPX_NOERROR : SPXERR_NOT_FOUND;
}

SPXHR phrase_list_grammar_add_phrase_impl(CSpxHandleTable<ISpxGrammar, SPXGRAMMARHANDLE>*, SPXGRAMMARHANDLE, std::shared_ptr<ISpxGrammar> ptr, SPXPHRASEHANDLE hphrase)
{
    auto phrase = CSpxApiManager::TryGetPtr<ISpxPhrase, SPXPHRASEHANDLE>(hphrase);
    auto phraselist = SpxQueryInterface<ISpxPhraseList>(ptr);

    auto ok = phrase != nullptr;
    if (ok) phraselist->AddPhrase(phrase);

    return ok ? SPX_NOERROR : SPXERR_INVALID_HANDLE;
}

SPXHR phrase_list_grammar_set_weight_impl(CSpxHandleTable<ISpxGrammar, SPXGRAMMARHANDLE>*, SPXGRAMMARHANDLE, std::shared_ptr<ISpxGrammar> ptr, double weight)
{
    auto phraselist = SpxQueryInterface<ISpxPhraseList>(ptr);
    phraselist->SetWeight(weight);
    return SPX_NOERROR;
}

SPXHR phrase_list_grammar_clear_impl(CSpxHandleTable<ISpxGrammar, SPXGRAMMARHANDLE>*, SPXGRAMMARHANDLE, std::shared_ptr<ISpxGrammar> ptr)
{
    auto phraselist = SpxQueryInterface<ISpxPhraseList>(ptr);
    phraselist->Clear();
    return SPX_NOERROR;
}

SPXHR grammar_phrase_create_from_text_impl(std::shared_ptr<ISpxPhrase> phrase, const char* text)
{
    phrase->InitPhrase(PAL::ToWString(text).c_str());
    return SPX_NOERROR;
}

SPXHR grammar_create_from_storage_id_impl(std::shared_ptr<ISpxStoredGrammar> grammar, const char *id)
{
    grammar->InitStoredGrammar(PAL::ToWString(id).c_str());
    return SPX_NOERROR;
}

SPXHR class_language_model_from_storage_id_impl(std::shared_ptr<ISpxClassLanguageModel> clm, const char *storageid)
{
    clm->InitClassLanguageModel(PAL::ToWString(storageid).c_str());
    return SPX_NOERROR;
}

SPXHR grammar_list_from_recognizer_impl(CSpxHandleTable<ISpxRecognizer, SPXRECOHANDLE>*, SPXRECOHANDLE, std::shared_ptr<ISpxRecognizer>, std::shared_ptr<ISpxGrammar> grammarList, SPXGRAMMARHANDLE* hgrammarList)
{
    *hgrammarList = CSpxApiManager::TrackHandle<ISpxGrammar, SPXGRAMMARHANDLE>(grammarList);
    return SPX_NOERROR;
}

SPXHR grammar_list_add_grammar_impl(CSpxHandleTable<ISpxGrammar, SPXGRAMMARHANDLE>* table, SPXGRAMMARHANDLE, std::shared_ptr<ISpxGrammar>, std::shared_ptr<ISpxGrammarList> grammarList, SPXGRAMMARHANDLE hgrammar)
{
    auto grammar = table->TryGetPtr(hgrammar);
 
    auto ok = grammar != nullptr;
    if (ok) grammarList->AddGrammar(grammar);
 
    return ok ? SPX_NOERROR : SPXERR_INVALID_HANDLE;
}

SPXHR grammar_list_set_recognition_factor_impl(CSpxHandleTable<ISpxGrammar, SPXGRAMMARHANDLE>*, SPXGRAMMARHANDLE, std::shared_ptr<ISpxGrammar>, std::shared_ptr<ISpxGrammarList> ptr2, double factor)
{
    ptr2->SetRecognitionFactor(factor);
    return SPX_NOERROR;
}

SPXHR class_language_model_assign_class_impl(CSpxHandleTable<ISpxGrammar, SPXGRAMMARHANDLE>* table, SPXGRAMMARHANDLE, std::shared_ptr<ISpxGrammar>, std::shared_ptr<ISpxClassLanguageModel> ptr2, const char* classname, SPXGRAMMARHANDLE hgrammar)
{
    auto grammar = table->TryGetPtr(hgrammar);

    auto ok = grammar != nullptr;
    if (ok) ptr2->AssignClass(PAL::ToWString(classname).c_str(), grammar);
    
    return ok ? SPX_NOERROR : SPXERR_INVALID_HANDLE;
}

SPXAPI_(bool) grammar_handle_is_valid(SPXGRAMMARHANDLE hgrammar)
{
    return CSpxApiManager::IsValid<SPXGRAMMARHANDLE, ISpxGrammar>(hgrammar);
}

SPXAPI grammar_handle_release(SPXGRAMMARHANDLE hgrammar)
{
    return CSpxApiManager::ReleaseAlwaysNoError<SPXGRAMMARHANDLE, ISpxGrammar>(hgrammar);
}

SPXAPI phrase_list_grammar_from_recognizer_by_name(SPXGRAMMARHANDLE* hgrammar, SPXRECOHANDLE hreco, const char* name)
{
    SPX_IFTRUE(hgrammar != nullptr, *hgrammar = SPXHANDLE_INVALID); // don't return early if nullptr (size optimization), but guarantee *hgrammar=nullptr (if possible)
    return hreco != nullptr && name != nullptr
        ? CSpxApiManager::PtrQiFn<SPXRECOHANDLE, ISpxRecognizer, ISpxGrammarList, const char*, SPXGRAMMARHANDLE*>(hreco, phrase_list_grammar_from_recognizer_by_name_impl, name, hgrammar)
        : SPXERR_INVALID_ARG;
}

SPXAPI phrase_list_grammar_add_phrase(SPXGRAMMARHANDLE hgrammar, SPXPHRASEHANDLE hphrase)
{
    return hphrase != nullptr
        ? CSpxApiManager::PtrFn<SPXGRAMMARHANDLE, ISpxGrammar>(hgrammar, phrase_list_grammar_add_phrase_impl, hphrase)
        : SPXERR_INVALID_ARG;
}

SPXAPI phrase_list_grammar_add_phrase_from_string(SPXGRAMMARHANDLE hgrammar, const char* phrase)
{
    SPXAPI_INIT_HR_TRY(hr)
    {
        auto grammar = CSpxApiManager::TryGetPtr<ISpxGrammar, SPXGRAMMARHANDLE>(hgrammar);
        SPX_THROW_HR_IF(SPXERR_INVALID_HANDLE, nullptr == grammar);

        auto phraseList = SpxQueryInterface<ISpxPhraseList>(grammar);
        SPX_THROW_HR_IF(SPXERR_INVALID_HANDLE, nullptr == phraseList);

        phraseList->AddPhrase(phrase);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI phrase_list_grammar_set_weight(SPXGRAMMARHANDLE hgrammar, double weight)
{
    return weight >= 0 && weight <= 2 // allowed range [0.0, 2.0]
        ? CSpxApiManager::PtrFn<SPXGRAMMARHANDLE, ISpxGrammar, double>(hgrammar, phrase_list_grammar_set_weight_impl, weight)
        : SPXERR_INVALID_ARG;
}

SPXAPI phrase_list_grammar_clear(SPXGRAMMARHANDLE hgrammar)
{
    return CSpxApiManager::PtrFn<SPXGRAMMARHANDLE, ISpxGrammar>(hgrammar, phrase_list_grammar_clear_impl);
}

SPXAPI_(bool) grammar_phrase_handle_is_valid(SPXPHRASEHANDLE hphrase)
{
    return CSpxApiManager::IsValid<SPXPHRASEHANDLE, ISpxPhrase>(hphrase);
}

SPXAPI grammar_phrase_handle_release(SPXPHRASEHANDLE hphrase)
{
    return CSpxApiManager::ReleaseAlwaysNoError<SPXPHRASEHANDLE, ISpxPhrase>(hphrase);
}

SPXAPI grammar_phrase_create_from_text(SPXPHRASEHANDLE* hphrase, const char* text)
{
    SPX_IFTRUE(hphrase != nullptr, *hphrase = SPXHANDLE_INVALID); // don't return early if nullptr (size optimization), but guarantee *hphrase=nullptr (if possible)
    return hphrase != nullptr && text != nullptr
        ? CSpxApiManager::CreateFnTrack<SPXPHRASEHANDLE, ISpxPhrase, const char*>("CSpxPhrase", SpxGetRootSite, hphrase, grammar_phrase_create_from_text_impl, text)
        : SPXERR_INVALID_ARG;
}

SPXAPI grammar_create_from_storage_id(SPXGRAMMARHANDLE *hgrammar, const char *id)
{
    SPX_IFTRUE(hgrammar != nullptr, *hgrammar = SPXHANDLE_INVALID); // don't return early if nullptr (size optimization), but guarantee *hgrammar=nullptr (if possible)
    return hgrammar != nullptr && id != nullptr && id[0] != '\0'
        ? CSpxApiManager::CreateFnQiTrack<SPXGRAMMARHANDLE, ISpxStoredGrammar, ISpxGrammar, const char*>("CSpxStoredGrammar", SpxGetRootSite, hgrammar, grammar_create_from_storage_id_impl, id)
        : SPXERR_INVALID_ARG;
}

SPXAPI grammar_list_from_recognizer(SPXGRAMMARHANDLE *hgrammarlist, SPXRECOHANDLE hreco)
{
    SPX_IFTRUE(hgrammarlist != nullptr, *hgrammarlist = SPXHANDLE_INVALID); // don't return early if nullptr (size optimization), but guarantee *hgrammarlist=nullptr (if possible)
    return hgrammarlist != nullptr // PtrQiFn validates hreco (checking here not needed)
        ? CSpxApiManager::PtrQiFn<SPXRECOHANDLE, ISpxRecognizer, ISpxGrammar, SPXGRAMMARHANDLE*>(hreco, grammar_list_from_recognizer_impl, hgrammarlist)
        : SPXERR_INVALID_ARG;
}

SPXAPI grammar_list_add_grammar(SPXGRAMMARHANDLE hgrammarlist, SPXGRAMMARHANDLE hgrammar)
{
    return hgrammar != nullptr // PtrQiFn validates hgrammarlist (checking here not needed)
        ? CSpxApiManager::PtrQiFn<SPXGRAMMARHANDLE, ISpxGrammar, ISpxGrammarList, SPXGRAMMARHANDLE>(hgrammarlist, grammar_list_add_grammar_impl, hgrammar)
        : SPXERR_INVALID_ARG;
}

SPXAPI grammar_list_set_recognition_factor(SPXGRAMMARHANDLE hgrammarlist, double factor, GrammarList_RecognitionFactorScope scope)
{
    return factor >= 0 && scope == GrammarList_RecognitionFactorScope::PartialPhrase
        ? CSpxApiManager::PtrQiFn<SPXGRAMMARHANDLE, ISpxGrammar, ISpxGrammarList, double>(hgrammarlist, grammar_list_set_recognition_factor_impl, factor)
        : SPXERR_INVALID_ARG;
}

SPXAPI class_language_model_from_storage_id(SPXGRAMMARHANDLE* hclm, const char *storageid)
{
    SPX_IFTRUE(hclm != nullptr, *hclm = SPXHANDLE_INVALID); // don't return early if nullptr (size optimization), but guarantee *hclm=nullptr (if possible)
    return hclm != nullptr && storageid != nullptr && storageid[0] != '\0'
        ? CSpxApiManager::CreateFnQiTrack<SPXGRAMMARHANDLE, ISpxClassLanguageModel, ISpxGrammar, const char*>("CSpxClassLanguageModel", SpxGetRootSite, hclm, class_language_model_from_storage_id_impl, storageid)
        : SPXERR_INVALID_ARG;
}

SPXAPI class_language_model_assign_class(SPXGRAMMARHANDLE hclm, const char *classname, SPXGRAMMARHANDLE hgrammar)
{
    return classname != nullptr && classname[0] != '\0' && hgrammar != nullptr
        ? CSpxApiManager::PtrQiFn<SPXGRAMMARHANDLE, ISpxGrammar, ISpxClassLanguageModel, const char*, SPXGRAMMARHANDLE>(hclm, class_language_model_assign_class_impl, classname, hgrammar)
        : SPXERR_INVALID_ARG;
}
