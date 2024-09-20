/*
 *  This file is part of cake compiler
 *  https://github.com/thradams/cake
*/

#pragma safety enable

#include "ownership.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "visit.h"
#include "expressions.h"

/*imagine tou press DEL key*/
static void del(struct token* from, struct token* to)
{
    struct token* _Opt p = from;
    while (p)
    {
        p->flags |= TK_C_BACKEND_FLAG_HIDE;

        if (p == to)
            break;
        p = p->next;
    }
}

/*imagine tou press CUT key - (tokens are never removed, they become invisible)*/
static struct token_list cut(struct token* from, struct token* to)
{
    struct token_list l = { 0 };
    try
    {
        struct token* _Opt p = from;
        while (p)
        {
            if (p->level == 0 &&
                !(p->flags & TK_FLAG_MACRO_EXPANDED) &&
                !(p->flags & TK_C_BACKEND_FLAG_HIDE) &&
                p->type != TK_BEGIN_OF_FILE)
            {
                struct token* _Owner _Opt clone = clone_token(p);
                if (clone == NULL) throw;

                p->flags |= TK_C_BACKEND_FLAG_HIDE;
                token_list_add(&l, clone);
                if (p == to)
                    break;
            }

            if (p == to)
                break;
            p = p->next;
        }
    }
    catch
    {
    }
    return l;
}


void defer_scope_delete_all(struct defer_scope* _Owner p);

void visit_ctx_destroy(struct visit_ctx* _Obj_owner ctx)
{
    if (ctx->tail_block)
        defer_scope_delete_all(ctx->tail_block);

    token_list_destroy(&ctx->insert_before_declaration);
    token_list_destroy(&ctx->insert_before_block_item);
}

static void visit_attribute_specifier_sequence(struct visit_ctx* ctx, struct attribute_specifier_sequence* p_visit_attribute_specifier_sequence);
static void visit_declaration(struct visit_ctx* ctx, struct declaration* p_declaration);

struct token* _Opt next_parser_token(struct token* token)
{
    struct token* _Opt r = token->next;

    while (r && !(r->flags & TK_FLAG_FINAL))
    {
        r = r->next;
    }
    return r;
}

static void visit_struct_or_union_specifier(struct visit_ctx* ctx, struct struct_or_union_specifier* p_struct_or_union_specifier);
static void visit_expression(struct visit_ctx* ctx, struct expression* p_expression);
static void visit_statement(struct visit_ctx* ctx, struct statement* p_statement);
static void visit_enum_specifier(struct visit_ctx* ctx, struct enum_specifier* p_enum_specifier);
static void visit_type_specifier(struct visit_ctx* ctx, struct type_specifier* p_type_specifier);

void convert_if_statement(struct visit_ctx* ctx, struct selection_statement* p_selection_statement)
{
    /*
      OBS:
      To debug this code use
      print_code_as_we_see(&ctx->ast.token_list, false);
      before and after each transformation
    */

    if (p_selection_statement->p_init_statement == NULL &&
        p_selection_statement->condition != NULL &&
        p_selection_statement->condition->expression)
    {
        return;
    }


    token_list_paste_string_before(&ctx->ast.token_list, p_selection_statement->first_token, "{");


    struct token_list init_tokens_cut = { 0 };
    if (p_selection_statement->p_init_statement &&
        p_selection_statement->p_init_statement->p_expression_statement)
    {
        if (p_selection_statement->p_init_statement->p_expression_statement->expression_opt)
        {
            init_tokens_cut = cut(p_selection_statement->p_init_statement->p_expression_statement->expression_opt->first_token,
                p_selection_statement->p_init_statement->p_expression_statement->expression_opt->last_token);
        }
        else
        {
            assert(false);
        }
    }
    else if (p_selection_statement->p_init_statement &&
        p_selection_statement->p_init_statement->p_simple_declaration)
    {
        init_tokens_cut = cut(p_selection_statement->p_init_statement->p_simple_declaration->first_token,
            p_selection_statement->p_init_statement->p_simple_declaration->last_token);
    }

    token_list_insert_before(&ctx->ast.token_list, p_selection_statement->first_token, &init_tokens_cut);


    struct token_list condition_tokens_cut = { 0 };
    if (p_selection_statement->condition && p_selection_statement->condition->expression)
    {
        /*leave it */
    }
    else if (p_selection_statement->condition &&
        p_selection_statement->condition->p_declaration_specifiers)
    {
        condition_tokens_cut = cut(p_selection_statement->condition->first_token,
            p_selection_statement->condition->last_token);

        token_list_insert_before(&ctx->ast.token_list, p_selection_statement->first_token, &condition_tokens_cut);
        token_list_paste_string_before(&ctx->ast.token_list, p_selection_statement->first_token, ";");

        if (p_selection_statement->condition->p_init_declarator &&
            p_selection_statement->condition->p_init_declarator->p_declarator->name_opt)
        {
            token_list_paste_string_before(&ctx->ast.token_list, p_selection_statement->close_parentesis_token,
                p_selection_statement->condition->p_init_declarator->p_declarator->name_opt->lexeme
            );
        }
        else
        {
            assert(false);
        }


    }

    token_list_paste_string_after(&ctx->ast.token_list, p_selection_statement->last_token, "}");
    token_list_destroy(&condition_tokens_cut);
    token_list_destroy(&init_tokens_cut);
}

void print_block_defer(struct defer_scope* defer_block, struct osstream* ss, bool hide_tokens)
{
    struct defer_scope* _Opt defer_child = defer_block->lastchild;
    while (defer_child != NULL)
    {
        if (defer_child->defer_statement == NULL)
        {
            assert(false);
            return;
        }

        _View struct token_list l = { 0 };

        l.head = defer_child->defer_statement->first_token;
        l.tail = defer_child->defer_statement->last_token;

        l.head->flags |= TK_C_BACKEND_FLAG_HIDE;
        const char* _Owner _Opt s = get_code_as_compiler_see(&l);
        if (s != NULL)
        {
            if (hide_tokens)
                token_range_add_flag(l.head, l.tail, TK_C_BACKEND_FLAG_HIDE);

            ss_fprintf(ss, "%s", s);
            free((void* _Owner)s);
        }
        defer_child = defer_child->previous;
    }
}


void hide_block_defer(struct defer_scope* deferblock)
{
    struct defer_scope* _Opt deferchild = deferblock->lastchild;
    while (deferchild != NULL)
    {
        if (deferchild->defer_statement == NULL)
        {
            assert(false);
            return;
        }

        struct token* head = deferchild->defer_statement->first_token;
        struct token* tail = deferchild->defer_statement->last_token;
        token_range_add_flag(head, tail, TK_C_BACKEND_FLAG_HIDE);
        deferchild = deferchild->previous;
    }
}


void print_all_defer_until_try(struct defer_scope* deferblock, struct osstream* ss)
{
    struct defer_scope* _Opt p_defer = deferblock;
    while (p_defer != NULL)
    {
        print_block_defer(p_defer, ss, false);

        if (p_defer->p_try_statement)
        {
            break;
        }

        p_defer = p_defer->previous;
    }
}

bool find_label_statement(struct statement* statement, const char* label);
bool find_label_unlabeled_statement(struct unlabeled_statement* p_unlabeled_statement, const char* label)
{
    if (p_unlabeled_statement->primary_block)
    {
        if (p_unlabeled_statement->primary_block->compound_statement)
        {
            struct block_item* _Opt block_item =
                p_unlabeled_statement->primary_block->compound_statement->block_item_list.head;
            while (block_item)
            {
                if (block_item->label &&
                    block_item->label->p_identifier_opt &&
                    strcmp(block_item->label->p_identifier_opt->lexeme, label) == 0)
                {
                    /*achou*/
                    return true;
                }
                else if (block_item->unlabeled_statement)
                {
                    if (find_label_unlabeled_statement(block_item->unlabeled_statement, label))
                    {
                        return true;
                    }
                }

                block_item = block_item->next;
            }
        }
        else if (p_unlabeled_statement->primary_block->selection_statement)
        {
            if (find_label_statement(p_unlabeled_statement->primary_block->selection_statement->secondary_block->statement, label))
            {
                return true;
            }
            if (p_unlabeled_statement->primary_block->selection_statement->else_secondary_block_opt)
            {
                if (find_label_statement(p_unlabeled_statement->primary_block->selection_statement->else_secondary_block_opt->statement, label))
                {
                    return true;
                }
            }
        }
        else if (p_unlabeled_statement->primary_block->try_statement)
        {
            if (find_label_statement(p_unlabeled_statement->primary_block->try_statement->secondary_block->statement, label))
            {
                return true;
            }
            if (p_unlabeled_statement->primary_block->try_statement->catch_secondary_block_opt)
            {
                if (find_label_statement(p_unlabeled_statement->primary_block->try_statement->catch_secondary_block_opt->statement, label))
                {
                    return true;
                }
            }
        }
        else if (p_unlabeled_statement->primary_block->iteration_statement)
        {
            if (find_label_statement(p_unlabeled_statement->primary_block->iteration_statement->secondary_block->statement, label))
            {
                return true;
            }
        }
    }
    return false;
}

bool find_label_statement(struct statement* statement, const char* label)
{
    if (statement->labeled_statement &&
        statement->labeled_statement->label->p_identifier_opt)
    {
        if (strcmp(statement->labeled_statement->label->p_identifier_opt->lexeme, label) == 0)
        {
            return true;
        }
    }
    else if (statement->unlabeled_statement)
    {
        if (find_label_unlabeled_statement(statement->unlabeled_statement, label))
            return true;
    }
    return false;
}

static bool find_label_scope(struct defer_scope* deferblock, const char* label)
{
    if (deferblock->p_iteration_statement)
    {

        if (find_label_statement(deferblock->p_iteration_statement->secondary_block->statement, label))
            return true;

    }
    else if (deferblock->p_selection_statement)
    {
        if (find_label_statement(deferblock->p_selection_statement->secondary_block->statement, label))
            return true;

        if (deferblock->p_selection_statement->else_secondary_block_opt)
        {
            if (find_label_statement(deferblock->p_selection_statement->else_secondary_block_opt->statement, label))
                return true;
        }
    }
    else if (deferblock->p_try_statement)
    {

        if (find_label_statement(deferblock->p_try_statement->secondary_block->statement, label))
            return true;


        if (deferblock->p_try_statement->catch_secondary_block_opt)
        {
            if (find_label_statement(deferblock->p_try_statement->catch_secondary_block_opt->statement, label))
                return true;
        }
    }
    else if (deferblock->p_statement)
    {
        if (find_label_statement(deferblock->p_statement, label))
            return true;
    }
    return false;
}

void print_all_defer_until_label(struct defer_scope* deferblock, const char* label, struct osstream* ss)
{
    /*
    * Precisamos saber quantos escopos nós saimos até achar o label.
    * Para isso procuramos no escopo atual aonde aparede o goto.
    * Se o label não esta diretamente neste escopo ou dentro de algum escopo interno
    * Não nós imprimos os defers pois estamos saindo do escopo e vamos para o escopo
    * de cima. Assim vamos repetindo em cada saida de escopo imprimos o defer.
    */
    struct defer_scope* _Opt p_defer = deferblock;

    while (p_defer != NULL)
    {
        if (!find_label_scope(p_defer, label))
        {
            print_block_defer(p_defer, ss, false);
        }
        else
        {
            break;
        }
        p_defer = p_defer->previous;
    }

}

void print_all_defer_until_iter(struct defer_scope* deferblock, struct osstream* ss)
{
    struct defer_scope* _Opt p_defer = deferblock;
    while (p_defer != NULL)
    {
        print_block_defer(p_defer, ss, false);
        if (p_defer->p_iteration_statement)
        {
            break;
        }
        p_defer = p_defer->previous;
    }
}

void print_all_defer_until_end(struct defer_scope* deferblock, struct osstream* ss)
{
    struct defer_scope* _Opt p_defer = deferblock;
    while (p_defer != NULL)
    {
        print_block_defer(p_defer, ss, false);
        p_defer = p_defer->previous;
    }
}

static void visit_secondary_block(struct visit_ctx* ctx, struct secondary_block* p_secondary_block)
{
    visit_statement(ctx, p_secondary_block->statement);
}
struct defer_scope* _Opt visit_ctx_push_tail_child(struct visit_ctx* ctx)
{
    struct defer_scope* _Owner _Opt p_defer = calloc(1, sizeof * p_defer);
    if (p_defer == NULL)
        return NULL;

    if (ctx->tail_block == NULL)
    {
        ctx->tail_block = p_defer;
        return ctx->tail_block;
    }

    p_defer->previous = ctx->tail_block->lastchild;
    ctx->tail_block->lastchild = p_defer;
    return ctx->tail_block->lastchild;
}


struct defer_scope* _Opt visit_ctx_push_tail_block(struct visit_ctx* ctx)
{
    struct defer_scope* _Owner _Opt p_defer = calloc(1, sizeof * p_defer);
    if (p_defer == NULL)
        return NULL;

    p_defer->previous = ctx->tail_block;
    ctx->tail_block = p_defer;

    return ctx->tail_block;
}
static void visit_defer_statement(struct visit_ctx* ctx, struct defer_statement* p_defer_statement)
{
    if (!ctx->is_second_pass)
    {
        //add as child of the last block
        struct defer_scope* _Opt p_defer = visit_ctx_push_tail_child(ctx);
        if (p_defer == NULL)
            return;

        p_defer->defer_statement = p_defer_statement;

        visit_secondary_block(ctx, p_defer_statement->secondary_block);
    }
    else //if (ctx->is_second_pass)
    {
        visit_secondary_block(ctx, p_defer_statement->secondary_block);
    }
}



void defer_scope_delete_one(struct defer_scope* _Owner p_block);

void visit_ctx_pop_tail_block(struct visit_ctx* ctx)
{
    if (ctx->tail_block)
    {
        struct defer_scope* _Owner _Opt previous = ctx->tail_block->previous;
        ctx->tail_block->previous = NULL;
        defer_scope_delete_one(ctx->tail_block);
        ctx->tail_block = previous;
    }
}

static void visit_try_statement(struct visit_ctx* ctx, struct try_statement* p_try_statement)
{
    struct osstream ss = { 0 };
    try
    {
        if (!ctx->is_second_pass)
        {
            struct defer_scope* _Opt p_defer = visit_ctx_push_tail_block(ctx);
            if (p_defer == NULL)
                return;

            p_defer->p_try_statement = p_try_statement;

            visit_secondary_block(ctx, p_try_statement->secondary_block);

            print_block_defer(p_defer, &ss, true);

            if (ss.c_str == NULL)
            {
                throw;
            }

            struct tokenizer_ctx tctx = { 0 };
            struct token_list l = tokenizer(&tctx, ss.c_str, NULL, 0, TK_FLAG_FINAL);
            token_list_insert_after(&ctx->ast.token_list, p_try_statement->secondary_block->last_token->prev, &l);


            visit_ctx_pop_tail_block(ctx);
            char* _Opt _Owner temp0 = strdup("if (1) /*try*/");
            if (temp0 == NULL)
            {
                token_list_destroy(&l);
                throw;
            }

            free(p_try_statement->first_token->lexeme);
            p_try_statement->first_token->lexeme = temp0;


            char buffer[50] = { 0 };
            if (p_try_statement->catch_secondary_block_opt)
            {
                assert(p_try_statement->catch_token_opt != NULL);

                snprintf(buffer, sizeof buffer, "else _catch_label_%d:", p_try_statement->try_catch_block_index);

                char* _Opt _Owner temp = strdup(buffer);
                if (temp == NULL)
                {
                    token_list_destroy(&l);
                    throw;
                }

                free(p_try_statement->catch_token_opt->lexeme);
                p_try_statement->catch_token_opt->lexeme = temp;

                visit_secondary_block(ctx, p_try_statement->catch_secondary_block_opt);
            }
            else
            {


                snprintf(buffer, sizeof buffer, "} else {_catch_label_%d:;}", p_try_statement->try_catch_block_index);

                char* _Opt _Owner temp = strdup(buffer);
                if (temp == NULL)
                {
                    token_list_destroy(&l);
                    throw;
                }

                free(p_try_statement->last_token->lexeme);
                p_try_statement->last_token->lexeme = temp;
            }


            token_list_destroy(&l);
        }
    }
    catch
    {
    }
    ss_close(&ss);
}

static void visit_declaration_specifiers(struct visit_ctx* ctx,
    struct declaration_specifiers* p_declaration_specifiers,
    struct type* _Opt p_type);

static void visit_init_declarator_list(struct visit_ctx* ctx, struct init_declarator_list* p_init_declarator_list);

static void visit_simple_declaration(struct visit_ctx* ctx, struct simple_declaration* p_simple_declaration)
{
    if (p_simple_declaration->p_attribute_specifier_sequence_opt)
        visit_attribute_specifier_sequence(ctx, p_simple_declaration->p_attribute_specifier_sequence_opt);
    visit_declaration_specifiers(ctx, p_simple_declaration->p_declaration_specifiers, NULL);
    visit_init_declarator_list(ctx, &p_simple_declaration->init_declarator_list);
}
static void visit_expression_statement(struct visit_ctx* ctx, struct expression_statement* p_expression_statement);

static void visit_init_statement(struct visit_ctx* ctx, struct init_statement* p_init_statement)
{
    if (p_init_statement->p_expression_statement)
        visit_expression_statement(ctx, p_init_statement->p_expression_statement);
    if (p_init_statement->p_simple_declaration)
        visit_simple_declaration(ctx, p_init_statement->p_simple_declaration);
}

static void visit_initializer(struct visit_ctx* ctx, struct initializer* p_initializer);

static void visit_declarator(struct visit_ctx* ctx, struct declarator* p_declarator);

static void visit_init_declarator(struct visit_ctx* ctx, struct init_declarator* p_init_declarator)
{
    visit_declarator(ctx, p_init_declarator->p_declarator);

    if (p_init_declarator->initializer)
        visit_initializer(ctx, p_init_declarator->initializer);
}
static void visit_condition(struct visit_ctx* ctx, struct condition* p_condition)
{
    if (p_condition->p_declaration_specifiers)
    {
        visit_declaration_specifiers(ctx,
            p_condition->p_declaration_specifiers,
            &p_condition->p_init_declarator->p_declarator->type);
    }

    if (p_condition->p_init_declarator)
        visit_init_declarator(ctx, p_condition->p_init_declarator);


    if (p_condition->expression)
        visit_expression(ctx, p_condition->expression);
}

static void visit_selection_statement(struct visit_ctx* ctx, struct selection_statement* p_selection_statement)
{
    struct defer_scope* _Opt p_defer = visit_ctx_push_tail_block(ctx);
    if (p_defer == NULL)
        return;

    p_defer->p_selection_statement = p_selection_statement;

    if (p_selection_statement->p_init_statement)
        visit_init_statement(ctx, p_selection_statement->p_init_statement);

    if (p_selection_statement->condition)
        visit_condition(ctx, p_selection_statement->condition);


    visit_secondary_block(ctx, p_selection_statement->secondary_block);

    struct osstream ss = { 0 };
    print_block_defer(p_defer, &ss, true);

    if (ss.c_str == NULL)
        return;

    if (ss.size > 0)
    {
        struct tokenizer_ctx tctx = { 0 };
        struct token_list l = tokenizer(&tctx, ss.c_str, NULL, 0, TK_FLAG_FINAL);
        token_list_insert_after(&ctx->ast.token_list, p_selection_statement->secondary_block->last_token->prev, &l);
        token_list_destroy(&l);
    }

    visit_ctx_pop_tail_block(ctx);

    if (p_selection_statement->else_secondary_block_opt)
        visit_secondary_block(ctx, p_selection_statement->else_secondary_block_opt);

    ss_close(&ss);

    //afte all visits and changes we visit again
    if (ctx->target < LANGUAGE_C2Y)
    {
        convert_if_statement(ctx, p_selection_statement);
    }
}

static void visit_compound_statement(struct visit_ctx* ctx, struct compound_statement* p_compound_statement);



char* _Opt remove_char(char* input, char ch)
{
    char* p_write = input;
    const char* p = input;
    while (*p)
    {
        if (p[0] == ch)
        {
            p++;
        }
        else
        {
            *p_write = *p;
            p++;
            p_write++;
        }
    }
    *p_write = 0;
    return input;
}

static void visit_initializer_list(struct visit_ctx* ctx, struct initializer_list* p_initializer_list);

static void visit_bracket_initializer_list(struct visit_ctx* ctx, struct braced_initializer* p_bracket_initializer_list)
{
    if (p_bracket_initializer_list->initializer_list == NULL)
    {
        if (ctx->target < LANGUAGE_C23)
        {
            assert(p_bracket_initializer_list->first_token->type == '{');

            const int level = p_bracket_initializer_list->first_token->level;
            //Criar token 0
            struct tokenizer_ctx tctx = { 0 };
            struct token_list list2 = tokenizer(&tctx, "0", NULL, level, TK_FLAG_FINAL);

            //inserir na frente
            token_list_insert_after(&ctx->ast.token_list, p_bracket_initializer_list->first_token, &list2);
            token_list_destroy(&list2);
        }
    }
    else
    {
        visit_initializer_list(ctx, p_bracket_initializer_list->initializer_list);
    }
}

static void visit_designation(struct visit_ctx* ctx, struct designation* p_designation)
{}

static void visit_initializer(struct visit_ctx* ctx, struct initializer* p_initializer)
{
    if (p_initializer->designation)
    {
        visit_designation(ctx, p_initializer->designation);
    }

    if (p_initializer->assignment_expression)
    {
        visit_expression(ctx, p_initializer->assignment_expression);
    }
    else if (p_initializer->braced_initializer)
    {
        visit_bracket_initializer_list(ctx, p_initializer->braced_initializer);
    }
}

static void visit_initializer_list(struct visit_ctx* ctx, struct initializer_list* p_initializer_list)
{
    struct initializer* _Opt p_initializer = p_initializer_list->head;
    while (p_initializer)
    {
        visit_initializer(ctx, p_initializer);
        p_initializer = p_initializer->next;
    }
}

static void visit_type_qualifier(struct visit_ctx* ctx, struct type_qualifier* p_type_qualifier)
{
    if (ctx->target < LANGUAGE_C99 && p_type_qualifier->token->type == TK_KEYWORD_RESTRICT)
    {
        char* _Opt _Owner temp = strdup("/*restrict*/");
        if (temp == NULL)
            return;

        free(p_type_qualifier->token->lexeme);
        p_type_qualifier->token->lexeme = temp;
    }

    if (p_type_qualifier->token->type == TK_KEYWORD__OUT ||
        p_type_qualifier->token->type == TK_KEYWORD__OPT ||
        p_type_qualifier->token->type == TK_KEYWORD__OWNER ||
        p_type_qualifier->token->type == TK_KEYWORD__OBJ_OWNER ||
        p_type_qualifier->token->type == TK_KEYWORD__VIEW)
    {
        char temp[100] = { 0 };
        snprintf(temp, sizeof temp, "/*%s*/", p_type_qualifier->token->lexeme);

        char* _Opt _Owner s = strdup(temp);
        if (s == NULL)
            return;

        free(p_type_qualifier->token->lexeme);
        p_type_qualifier->token->lexeme = s;
    }
}

static void visit_specifier_qualifier(struct visit_ctx* ctx, struct type_specifier_qualifier* p_specifier_qualifier)
{
    if (p_specifier_qualifier->type_specifier)
        visit_type_specifier(ctx, p_specifier_qualifier->type_specifier);

    if (p_specifier_qualifier->type_qualifier)
        visit_type_qualifier(ctx, p_specifier_qualifier->type_qualifier);
}

static void visit_specifier_qualifier_list(struct visit_ctx* ctx,
    struct specifier_qualifier_list* p_specifier_qualifier_list,
    struct type* p_type)
{

    //(typeof(int[2])*)
    // 
    //TODO se tiver typeof em qualquer parte tem que imprimir todo  tipo
    // tem que refazer
    if (p_specifier_qualifier_list->type_specifier_flags & TYPE_SPECIFIER_TYPEOF)
    {
        const int level = p_specifier_qualifier_list->first_token->level;

        token_range_add_flag(p_specifier_qualifier_list->first_token,
            p_specifier_qualifier_list->last_token, TK_C_BACKEND_FLAG_HIDE);

        struct osstream ss = { 0 };
        print_type_no_names(&ss, type_get_specifer_part(p_type));

        if (ss.c_str == NULL)
            return;

        struct tokenizer_ctx tctx = { 0 };
        struct token_list l2 = tokenizer(&tctx, ss.c_str, NULL, level, TK_FLAG_FINAL);
        token_list_insert_after(&ctx->ast.token_list, p_specifier_qualifier_list->last_token, &l2);

        ss_close(&ss);
        token_list_destroy(&l2);
    }

    if (p_specifier_qualifier_list->struct_or_union_specifier)
    {
        visit_struct_or_union_specifier(ctx, p_specifier_qualifier_list->struct_or_union_specifier);
    }
    else if (p_specifier_qualifier_list->enum_specifier)
    {
        visit_enum_specifier(ctx, p_specifier_qualifier_list->enum_specifier);
    }
    else if (p_specifier_qualifier_list->typedef_declarator)
    {
        //typedef name
    }
    //else if (p_specifier_qualifier_list->p_typeof_expression_opt)
    //{
      //  visit_expression(ctx, p_specifier_qualifier_list->p_typeof_expression_opt);
    //}
    else
    {
        struct type_specifier_qualifier* _Opt p_specifier_qualifier = p_specifier_qualifier_list->head;
        while (p_specifier_qualifier)
        {
            visit_specifier_qualifier(ctx, p_specifier_qualifier);
            p_specifier_qualifier = p_specifier_qualifier->next;
        }
    }
}

static void visit_type_name(struct visit_ctx* ctx, struct type_name* p_type_name)
{

    visit_specifier_qualifier_list(ctx, p_type_name->specifier_qualifier_list, &p_type_name->type);
    visit_declarator(ctx, p_type_name->abstract_declarator);


    /*
    * Vamos esconder tudo e gerar um novo
    *  Exemplo
    *  (const typeof(int (*)())) -> *  ( int (*const )() )
    */
}



static void visit_argument_expression_list(struct visit_ctx* ctx, struct argument_expression_list* p_argument_expression_list)
{
    struct argument_expression* _Opt p_argument_expression = p_argument_expression_list->head;
    while (p_argument_expression)
    {
        visit_expression(ctx, p_argument_expression->expression);
        p_argument_expression = p_argument_expression->next;
    }
}

static void visit_generic_selection(struct visit_ctx* ctx, struct generic_selection* p_generic_selection)
{
    if (p_generic_selection->expression)
    {
        visit_expression(ctx, p_generic_selection->expression);
    }
    else if (p_generic_selection->type_name)
    {
        visit_type_name(ctx, p_generic_selection->type_name);
    }

    struct generic_association* _Opt p = p_generic_selection->generic_assoc_list.head;
    while (p)
    {
        if (p->p_type_name) visit_type_name(ctx, p->p_type_name);
        visit_expression(ctx, p->expression);
        p = p->next;
    }

    if (ctx->target < LANGUAGE_C11)
    {

        /*the select part will be temporally hidden*/
        if (p_generic_selection->p_view_selected_expression)
        {
            for (struct token* current = p_generic_selection->p_view_selected_expression->first_token;
                current != p_generic_selection->p_view_selected_expression->last_token->next;
                current = current->next)
            {
                if (!(current->flags & TK_C_BACKEND_FLAG_HIDE))
                {
                    current->flags |= TK_C_BACKEND_FLAG_SHOW_AGAIN;
                }
                if (current->next == NULL)
                {
                    break;
                }
            }
        }

        /*let's hide everything first*/
        token_range_add_flag(p_generic_selection->first_token, p_generic_selection->last_token, TK_C_BACKEND_FLAG_HIDE);

        /*lets show again just the part of the select that was visible*/
        if (p_generic_selection->p_view_selected_expression)
        {
            for (struct token* current = p_generic_selection->p_view_selected_expression->first_token;
                 current != p_generic_selection->p_view_selected_expression->last_token->next;
                 current = current->next)
            {
                if ((current->flags & TK_C_BACKEND_FLAG_HIDE) &&
                    (current->flags & TK_C_BACKEND_FLAG_SHOW_AGAIN))
                {
                    current->flags = current->flags & ~(TK_C_BACKEND_FLAG_SHOW_AGAIN | TK_C_BACKEND_FLAG_HIDE);
                }
                if (current->next == NULL)
                {
                    break;
                }
            }
        }
    }

}


static void visit_expression(struct visit_ctx* ctx, struct expression* p_expression)
{
    switch (p_expression->expression_type)
    {
    case EXPRESSION_TYPE_INVALID:
        assert(false);
        break;

    case PRIMARY_EXPRESSION__FUNC__:
        break;

    case PRIMARY_EXPRESSION_ENUMERATOR:
        if (ctx->target < LANGUAGE_C23)
        {
            struct type t = type_get_enum_type(&p_expression->type);
            if (t.type_specifier_flags != TYPE_SPECIFIER_INT)
            {
                struct osstream ss0 = { 0 };
                print_type(&ss0, &t);
                if (ss0.c_str == NULL)
                {
                    type_destroy(&t);
                    return;
                }

                struct osstream ss = { 0 };
                ss_fprintf(&ss, "((%s)%s)", ss0.c_str, p_expression->first_token->lexeme);
                if (ss.c_str == NULL)
                {
                    ss_close(&ss0);
                    type_destroy(&t);
                    return;
                }

                free(p_expression->first_token->lexeme);
                p_expression->first_token->lexeme = ss.c_str;
                ss.c_str = NULL; /*MOVED*/
                ss_close(&ss);
                ss_close(&ss0);
            }
            type_destroy(&t);
        }
        break;
    case PRIMARY_EXPRESSION_DECLARATOR:

        if (ctx->target < LANGUAGE_C23)
        {
            if (constant_value_is_valid(&p_expression->constant_value))
            {
                free((void* _Owner)p_expression->type.name_opt);
                p_expression->type.name_opt = NULL;

                struct osstream ss = { 0 };
                print_type(&ss, &p_expression->type);
                if (ss.c_str == NULL)
                    return;

                struct osstream ss1 = { 0 };

                /*
                  this is the way we handle constexpr, replacing the declarator
                  for it's number and changing the expression type
                  we are not handling &a at this moment
                */
                char buffer[40] = { 0 };
                constant_value_to_string(&p_expression->constant_value, buffer, sizeof buffer);

                ss_fprintf(&ss1, "((%s)%s)", ss.c_str, buffer);
                if (ss1.c_str == NULL)
                {
                    ss_close(&ss);
                    return;
                }

                free(p_expression->first_token->lexeme);
                p_expression->first_token->lexeme = ss1.c_str;
                ss1.c_str = NULL;// MOVED
                p_expression->expression_type = PRIMARY_EXPRESSION_NUMBER;

                ss_close(&ss);
                ss_close(&ss1);
            }
        }

        break;
    case PRIMARY_EXPRESSION_STRING_LITERAL:
        break;
    case PRIMARY_EXPRESSION_CHAR_LITERAL:
        break;
    case PRIMARY_EXPRESSION_NUMBER:
        break;

    case PRIMARY_EXPRESSION_PREDEFINED_CONSTANT:
        if (p_expression->first_token->type == TK_KEYWORD_NULLPTR)
        {
            if (ctx->target < LANGUAGE_C23)
            {
                char* _Opt _Owner temp = strdup("((void*)0)");
                if (temp == NULL)
                    return;

                free(p_expression->first_token->lexeme);
                p_expression->first_token->lexeme = temp;
            }
        }
        else if (p_expression->first_token->type == TK_KEYWORD_TRUE)
        {
            if (ctx->target < LANGUAGE_C99)
            {
                char* _Owner _Opt temp = strdup("1");;
                if (temp == NULL)
                    return;

                free(p_expression->first_token->lexeme);
                p_expression->first_token->lexeme = temp;
            }
            else if (ctx->target < LANGUAGE_C23)
            {
                char* _Opt _Owner temp = strdup("((_Bool)1)");
                if (temp == NULL)
                    return;

                free(p_expression->first_token->lexeme);
                p_expression->first_token->lexeme = temp;
            }
        }
        else if (p_expression->first_token->type == TK_KEYWORD_FALSE)
        {
            if (ctx->target < LANGUAGE_C99)
            {
                char* _Opt _Owner temp = strdup("0");
                if (temp == NULL)
                    return;

                free(p_expression->first_token->lexeme);
                p_expression->first_token->lexeme = temp;
            }
            else if (ctx->target < LANGUAGE_C23)
            {
                char* _Opt _Owner temp = strdup("((_Bool)0)");
                if (temp == NULL)
                    return;

                free(p_expression->first_token->lexeme);
                p_expression->first_token->lexeme = temp;
            }
        }
        break;

    case PRIMARY_EXPRESSION_PARENTESIS:
        assert(p_expression->right != NULL);
        visit_expression(ctx, p_expression->right);
        break;

    case PRIMARY_EXPRESSION_GENERIC:
        assert(p_expression->generic_selection != NULL);
        visit_generic_selection(ctx, p_expression->generic_selection);
        break;

    case POSTFIX_DOT:
    case POSTFIX_ARROW:
    case POSTFIX_INCREMENT:
    case POSTFIX_DECREMENT:
        if (p_expression->left)
            visit_expression(ctx, p_expression->left);
        if (p_expression->right)
            visit_expression(ctx, p_expression->right);
        break;
    case POSTFIX_ARRAY:
        //visit_expression(ctx, p_expression->left);
        break;
    case POSTFIX_FUNCTION_CALL:

        if (p_expression->left)
            visit_expression(ctx, p_expression->left);
        if (p_expression->right)
            visit_expression(ctx, p_expression->right);

        visit_argument_expression_list(ctx, &p_expression->argument_expression_list);
        break;
    case POSTFIX_EXPRESSION_FUNCTION_LITERAL:
    {
        assert(p_expression->compound_statement != NULL);
        assert(p_expression->type_name != NULL);

        ctx->has_lambda = true;
        ctx->is_inside_lambda = true;
        visit_type_name(ctx, p_expression->type_name);
        visit_compound_statement(ctx, p_expression->compound_statement);
        ctx->is_inside_lambda = false;

        if (ctx->is_second_pass)
        {
            /*no segundo passo nós copiamos a funcao*/
            char name[100] = { 0 };
            snprintf(name, sizeof name, " _lit_func_%d", ctx->lambdas_index);
            ctx->lambdas_index++;

            struct osstream ss = { 0 };



            bool is_first = true;
            print_type_qualifier_flags(&ss, &is_first, p_expression->type_name->abstract_declarator->type.type_qualifier_flags);
            print_type_specifier_flags(&ss, &is_first, p_expression->type_name->abstract_declarator->type.type_specifier_flags);


            free((void* _Owner)p_expression->type_name->abstract_declarator->type.name_opt);
            p_expression->type_name->abstract_declarator->type.name_opt = strdup(name);

            struct osstream ss0 = { 0 };

            print_type(&ss0, &p_expression->type_name->abstract_declarator->type);
            ss_fprintf(&ss, "static %s", ss0.c_str);

            ss_close(&ss0);

            if (ss.c_str == NULL)
                return;

            struct tokenizer_ctx tctx = { 0 };
            struct token_list l1 = tokenizer(&tctx, ss.c_str, NULL, 0, TK_FLAG_FINAL);

            token_list_append_list(&ctx->insert_before_declaration, &l1);
            ss_close(&ss);
            token_list_destroy(&l1);

            for (struct token* current = p_expression->compound_statement->first_token;
                current != p_expression->compound_statement->last_token->next;
                current = current->next)
            {
                token_list_clone_and_add(&ctx->insert_before_declaration, current);
                if (current->next == NULL)
                    break;
            }

            token_range_add_flag(p_expression->first_token, p_expression->last_token, TK_C_BACKEND_FLAG_HIDE);


            struct token_list l3 = tokenizer(&tctx, "\n\n", NULL, 0, TK_FLAG_NONE);
            token_list_append_list(&ctx->insert_before_declaration, &l3);
            token_list_destroy(&l3);

            struct token_list l2 = tokenizer(&tctx, name, NULL, 0, TK_FLAG_FINAL);
            token_list_insert_after(&ctx->ast.token_list, p_expression->last_token, &l2);
            token_list_destroy(&l2);
        }
    }
    break;

    case POSTFIX_EXPRESSION_COMPOUND_LITERAL:

        assert(p_expression->braced_initializer != NULL);
        if (p_expression->type_name)
        {
            visit_type_name(ctx, p_expression->type_name);
        }

        visit_bracket_initializer_list(ctx, p_expression->braced_initializer);

        assert(p_expression->left == NULL);
        assert(p_expression->right == NULL);

        break;

    case UNARY_EXPRESSION_ALIGNOF:

        if (ctx->target < LANGUAGE_C11)
        {
            const int level = p_expression->first_token->level;
            token_range_add_flag(p_expression->first_token, p_expression->last_token, TK_C_BACKEND_FLAG_HIDE);
            char buffer[30] = { 0 };
            snprintf(buffer, sizeof buffer, "%lld", constant_value_to_signed_long_long(&p_expression->constant_value));
            struct tokenizer_ctx tctx = { 0 };
            struct token_list l3 = tokenizer(&tctx, buffer, NULL, level, TK_FLAG_FINAL);
            if (l3.head == NULL)
            {
                return;
            }

            l3.head->flags = p_expression->last_token->flags;
            token_list_insert_after(&ctx->ast.token_list, p_expression->last_token, &l3);
            token_list_destroy(&l3);
        }

        if (p_expression->right)
        {
            visit_expression(ctx, p_expression->right);
        }

        if (p_expression->type_name)
        {
            /*sizeof*/
            visit_type_name(ctx, p_expression->type_name);
        }
        break;

    case UNARY_EXPRESSION_NELEMENTSOF_TYPE:

        del(p_expression->first_token, p_expression->first_token);

        struct tokenizer_ctx tctx = { 0 };


        if (p_expression->right)
        {
            visit_expression(ctx, p_expression->right);

            struct token_list l = { .head = p_expression->right->first_token,
                                    .tail = p_expression->right->last_token };

            const char* _Owner _Opt exprstr = get_code_as_we_see(&l, true);
            char buffer[200] = { 0 };
            snprintf(buffer, sizeof buffer, "sizeof(%s)/sizeof((%s)[0])", exprstr, exprstr);

            struct token_list l2 = tokenizer(&tctx, buffer, NULL, 0, TK_FLAG_FINAL);

            token_list_insert_before(&ctx->ast.token_list,
                p_expression->last_token,
                &l2);

            del(p_expression->right->first_token, p_expression->right->last_token);
            free((char* _Owner)exprstr);

            token_list_destroy(&l2);
        }

        if (p_expression->type_name)
        {
            visit_type_name(ctx, p_expression->type_name);

            if (constant_value_is_valid(&p_expression->constant_value))
            {
                int u = constant_value_to_unsigned_int(&p_expression->constant_value);

                char buffer[50] = { 0 };
                snprintf(buffer, sizeof buffer, "%d", u);

                struct token_list l2 = tokenizer(&tctx, buffer, NULL, 0, TK_FLAG_FINAL);

                token_list_insert_before(&ctx->ast.token_list,
                    p_expression->last_token,
                    &l2);

                del(p_expression->type_name->first_token, p_expression->type_name->last_token);


                token_list_destroy(&l2);
            }
            else
            {
                //error
            }
        }
        break;

    case UNARY_EXPRESSION_SIZEOF_EXPRESSION:
    case UNARY_EXPRESSION_SIZEOF_TYPE:
    case UNARY_EXPRESSION_INCREMENT:
    case UNARY_EXPRESSION_DECREMENT:

    case UNARY_EXPRESSION_NOT:
    case UNARY_EXPRESSION_BITNOT:
    case UNARY_EXPRESSION_NEG:
    case UNARY_EXPRESSION_PLUS:
    case UNARY_EXPRESSION_CONTENT:
    case UNARY_EXPRESSION_ADDRESSOF:
    case UNARY_EXPRESSION_ASSERT:

        if (p_expression->right)
        {
            visit_expression(ctx, p_expression->right);
        }

        if (p_expression->type_name)
        {
            /*sizeof*/
            visit_type_name(ctx, p_expression->type_name);
        }

        break;




    case ASSIGNMENT_EXPRESSION:
    case CAST_EXPRESSION:
    case MULTIPLICATIVE_EXPRESSION_MULT:
    case MULTIPLICATIVE_EXPRESSION_DIV:
    case MULTIPLICATIVE_EXPRESSION_MOD:
    case ADDITIVE_EXPRESSION_PLUS:
    case ADDITIVE_EXPRESSION_MINUS:
    case SHIFT_EXPRESSION_RIGHT:
    case SHIFT_EXPRESSION_LEFT:
    case RELATIONAL_EXPRESSION_BIGGER_THAN:
    case RELATIONAL_EXPRESSION_LESS_THAN:
    case EQUALITY_EXPRESSION_EQUAL:
    case EQUALITY_EXPRESSION_NOT_EQUAL:
    case AND_EXPRESSION:
    case EXCLUSIVE_OR_EXPRESSION:
    case INCLUSIVE_OR_EXPRESSION:

    case RELATIONAL_EXPRESSION_LESS_OR_EQUAL_THAN:
    case RELATIONAL_EXPRESSION_BIGGER_OR_EQUAL_THAN:

    case LOGICAL_AND_EXPRESSION:
    case LOGICAL_OR_EXPRESSION:


        if (p_expression->left)
        {
            visit_expression(ctx, p_expression->left);
        }
        if (p_expression->right)
        {
            visit_expression(ctx, p_expression->right);
        }
        if (p_expression->type_name)
        {
            visit_type_name(ctx, p_expression->type_name);
        }


        break;

    case UNARY_EXPRESSION_TRAITS:
    {
        if (ctx->target < LANGUAGE_CAK)
        {
            struct tokenizer_ctx tctx2 = { 0 };
            struct token_list l2 = { 0 };

            if (constant_value_to_bool(&p_expression->constant_value))
                l2 = tokenizer(&tctx2, "1", NULL, 0, TK_FLAG_FINAL);
            else
                l2 = tokenizer(&tctx2, "0", NULL, 0, TK_FLAG_FINAL);


            token_list_insert_after(&ctx->ast.token_list,
                p_expression->last_token,
                &l2);

            token_range_add_flag(p_expression->first_token,
                p_expression->last_token,
                TK_C_BACKEND_FLAG_HIDE);

            token_list_destroy(&l2);
        }
    }
    break;

    case UNARY_EXPRESSION_IS_SAME:
        break;

    case UNARY_DECLARATOR_ATTRIBUTE_EXPR:
        break;

    case CONDITIONAL_EXPRESSION:
        if (p_expression->condition_expr)
        {
            visit_expression(ctx, p_expression->condition_expr);
        }

        if (p_expression->left)
        {
            visit_expression(ctx, p_expression->left);
        }
        if (p_expression->right)
        {
            visit_expression(ctx, p_expression->right);
        }

        break;

        //default:
          //  break;
    }
}

static void visit_expression_statement(struct visit_ctx* ctx, struct expression_statement* p_expression_statement)
{
    if (p_expression_statement->expression_opt)
        visit_expression(ctx, p_expression_statement->expression_opt);
}

static void visit_block_item_list(struct visit_ctx* ctx, struct block_item_list* p_block_item_list);

static void visit_compound_statement(struct visit_ctx* ctx, struct compound_statement* p_compound_statement)
{
    visit_block_item_list(ctx, &p_compound_statement->block_item_list);
}

//
static void visit_iteration_statement(struct visit_ctx* ctx, struct iteration_statement* p_iteration_statement)
{

    if (p_iteration_statement->expression0)
    {
        visit_expression(ctx, p_iteration_statement->expression0);
    }

    if (p_iteration_statement->expression1)
    {
        visit_expression(ctx, p_iteration_statement->expression1);
    }

    if (p_iteration_statement->expression2)
    {
        visit_expression(ctx, p_iteration_statement->expression2);
    }


    struct defer_scope* _Opt p_defer = visit_ctx_push_tail_block(ctx);
    if (p_defer == NULL)
        return;

    p_defer->p_iteration_statement = p_iteration_statement;

    visit_secondary_block(ctx, p_iteration_statement->secondary_block);

    struct osstream ss = { 0 };
    print_block_defer(p_defer, &ss, true);

    if (ss.c_str == NULL)
    {
        return;
    }

    struct tokenizer_ctx tctx = { 0 };
    struct token_list l = tokenizer(&tctx, ss.c_str, NULL, 0, TK_FLAG_FINAL);
    token_list_insert_after(&ctx->ast.token_list, p_iteration_statement->secondary_block->last_token->prev, &l);


    visit_ctx_pop_tail_block(ctx);

    ss_close(&ss);
    token_list_destroy(&l);

}


static void visit_jump_statement(struct visit_ctx* ctx, struct jump_statement* p_jump_statement)
{

    if (p_jump_statement->first_token->type == TK_KEYWORD_THROW)
    {
        assert(ctx->tail_block != NULL);
        struct osstream ss0 = { 0 };
        print_all_defer_until_try(ctx->tail_block, &ss0);

        if (ss0.size > 0)
        {
            struct osstream ss = { 0 };
            ss_fprintf(&ss, "{ %s ", ss0.c_str);
            ss_fprintf(&ss, "goto _catch_label_%d;", p_jump_statement->try_catch_block_index);
            ss_fprintf(&ss, "}");
            if (ss.c_str == NULL)
            {
                ss_close(&ss0);
                return;
            }

            free(p_jump_statement->first_token->lexeme);
            p_jump_statement->first_token->lexeme = ss.c_str;


            p_jump_statement->last_token->flags |= TK_C_BACKEND_FLAG_HIDE;

        }
        else
        {
            struct osstream ss = { 0 };
            ss_fprintf(&ss, "goto _catch_label_%d", p_jump_statement->try_catch_block_index);
            if (ss.c_str == NULL)
            {
                ss_close(&ss0);
                return;
            }

            free(p_jump_statement->first_token->lexeme);
            p_jump_statement->first_token->lexeme = ss.c_str; /*MOVED*/
        }

        ss_close(&ss0);
    }
    else if (p_jump_statement->first_token->type == TK_KEYWORD_RETURN)
    {
        const bool constant_expression =
            p_jump_statement->expression_opt == NULL ||
            constant_value_is_valid(&p_jump_statement->expression_opt->constant_value);

        if (p_jump_statement->expression_opt)
            visit_expression(ctx, p_jump_statement->expression_opt);

        if (constant_expression)
        {
            assert(ctx->tail_block != NULL);
            struct osstream ss0 = { 0 };
            print_all_defer_until_end(ctx->tail_block, &ss0);

            if (ss0.size > 0)
            {
                struct osstream ss = { 0 };
                ss_fprintf(&ss, "{ %s ", ss0.c_str);
                ss_fprintf(&ss, "return");

                if (ss.c_str == NULL)
                {
                    ss_close(&ss0);
                    return;
                }

                free(p_jump_statement->first_token->lexeme);
                p_jump_statement->first_token->lexeme = ss.c_str;
                ss.c_str = NULL; /*MOVED*/

                char* _Opt _Owner temp = strdup(";}");
                if (temp == NULL)
                {
                    ss_close(&ss0);
                    return;
                }

                free(p_jump_statement->last_token->lexeme);
                p_jump_statement->last_token->lexeme = temp;
                ss_close(&ss);
            }
            ss_close(&ss0);
        }
        else
        {
            assert(ctx->tail_block != NULL);
            //defer must run after return
            struct osstream defer_str = { 0 };
            print_all_defer_until_end(ctx->tail_block, &defer_str);

            if (defer_str.c_str == NULL)
                return;

            struct type t = type_dup(&p_jump_statement->expression_opt->type);
            type_add_const(&t);
            type_remove_names(&t);
            struct osstream return_type_str = { 0 };
            print_type(&return_type_str, &t);

            struct osstream sst = { 0 };
            ss_fprintf(&sst, "{ ");
            ss_fprintf(&sst, " %s _tmp = ", return_type_str.c_str);

            ss_close(&return_type_str);

            if (sst.c_str == NULL)
            {
                type_destroy(&t);
                ss_close(&defer_str);
                return;
            }

            free(p_jump_statement->first_token->lexeme);
            p_jump_statement->first_token->lexeme = sst.c_str;

            sst.c_str = NULL; //moved
            struct osstream ss = { 0 };
            ss_fprintf(&ss, "; %s return _tmp;}", defer_str.c_str);
            if (ss.c_str == NULL)
            {
                ss_close(&defer_str);
                type_destroy(&t);
                return;
            }

            free(p_jump_statement->last_token->lexeme);
            p_jump_statement->last_token->lexeme = ss.c_str;
            ss.c_str = NULL; /*MOVED*/
            ss_close(&ss);
            type_destroy(&t);
            ss_close(&defer_str);
        }
    }
    else if (p_jump_statement->first_token->type == TK_KEYWORD_BREAK ||
        p_jump_statement->first_token->type == TK_KEYWORD_CONTINUE)
    {
        assert(ctx->tail_block != NULL);

        struct osstream ss0 = { 0 };
        print_all_defer_until_iter(ctx->tail_block, &ss0);
        if (ss0.size > 0)
        {
            struct osstream ss = { 0 };
            ss_fprintf(&ss, "{ %s ", ss0.c_str);
            ss_fprintf(&ss, "break;");
            ss_fprintf(&ss, "}");
            if (ss.c_str == NULL)
            {
                ss_close(&ss0);
                return;
            }

            free(p_jump_statement->first_token->lexeme);
            p_jump_statement->first_token->lexeme = ss.c_str;
            ss.c_str = NULL;

            p_jump_statement->last_token->flags |= TK_C_BACKEND_FLAG_HIDE;
            ss_close(&ss);
        }

        ss_close(&ss0);
    }
    else if (p_jump_statement->first_token->type == TK_KEYWORD_GOTO)
    {
        assert(p_jump_statement->label != NULL);
        assert(ctx->tail_block != NULL);

        struct osstream ss0 = { 0 };
        print_all_defer_until_label(ctx->tail_block, p_jump_statement->label->lexeme, &ss0);
        if (ss0.c_str == NULL)
            return;

        struct osstream ss = { 0 };
        ss_fprintf(&ss, "{ %s ", ss0.c_str);
        ss_fprintf(&ss, "goto");
        if (ss.c_str == NULL)
        {
            ss_close(&ss0);
            return;
        }

        free(p_jump_statement->first_token->lexeme);
        p_jump_statement->first_token->lexeme = ss.c_str;
        ss.c_str = NULL; /*MOVED*/

        char* _Owner _Opt temp = strdup(";}");
        if (temp == NULL)
        {
            ss_close(&ss0);
            ss_close(&ss);
            return;
        }

        free(p_jump_statement->last_token->lexeme);
        p_jump_statement->last_token->lexeme = temp;
        ss_close(&ss);
        ss_close(&ss0);
    }
    else
    {
        assert(false);
    }
}


static void visit_labeled_statement(struct visit_ctx* ctx, struct labeled_statement* p_labeled_statement)
{
    visit_statement(ctx, p_labeled_statement->statement);
}

static void visit_primary_block(struct visit_ctx* ctx, struct primary_block* p_primary_block)
{
    if (p_primary_block->defer_statement)
    {
        visit_defer_statement(ctx, p_primary_block->defer_statement);
    }
    else
    {
        if (p_primary_block->compound_statement)
        {
            visit_compound_statement(ctx, p_primary_block->compound_statement);
        }
        else if (p_primary_block->iteration_statement)
        {
            visit_iteration_statement(ctx, p_primary_block->iteration_statement);
        }
        else if (p_primary_block->selection_statement)
        {
            visit_selection_statement(ctx, p_primary_block->selection_statement);
        }
        else if (p_primary_block->try_statement)
        {
            visit_try_statement(ctx, p_primary_block->try_statement);
        }
    }

}

static void visit_unlabeled_statement(struct visit_ctx* ctx, struct unlabeled_statement* p_unlabeled_statement)
{
    if (p_unlabeled_statement->primary_block)
    {
        visit_primary_block(ctx, p_unlabeled_statement->primary_block);
    }
    else if (p_unlabeled_statement->expression_statement)
    {
        visit_expression_statement(ctx, p_unlabeled_statement->expression_statement);
    }
    else if (p_unlabeled_statement->jump_statement)
    {
        visit_jump_statement(ctx, p_unlabeled_statement->jump_statement);
    }
    else
    {
        assert(false);
    }
}



static void visit_statement(struct visit_ctx* ctx, struct statement* p_statement)
{
    if (p_statement->labeled_statement)
    {
        visit_labeled_statement(ctx, p_statement->labeled_statement);
    }
    else if (p_statement->unlabeled_statement)
    {
        visit_unlabeled_statement(ctx, p_statement->unlabeled_statement);
    }
}

static void visit_label(struct visit_ctx* ctx, struct label* p_label)
{
    //p_label->name
}
static void visit_block_item(struct visit_ctx* ctx, struct block_item* p_block_item)
{
    if (p_block_item->declaration)
    {
        visit_declaration(ctx, p_block_item->declaration);
    }
    else if (p_block_item->unlabeled_statement)
    {
        visit_unlabeled_statement(ctx, p_block_item->unlabeled_statement);
    }
    else if (p_block_item->label)
    {
        visit_label(ctx, p_block_item->label);
    }
    if (ctx->insert_before_block_item.head != NULL)
    {
        token_list_insert_after(&ctx->ast.token_list, p_block_item->first_token->prev, &ctx->insert_before_block_item);
    }
}

static void visit_block_item_list(struct visit_ctx* ctx, struct block_item_list* p_block_item_list)
{
    struct block_item* _Opt p_block_item = p_block_item_list->head;
    while (p_block_item)
    {
        visit_block_item(ctx, p_block_item);
        p_block_item = p_block_item->next;
    }
}

static void visit_pragma_declaration(struct visit_ctx* ctx, struct pragma_declaration* p_pragma_declaration)
{
    p_pragma_declaration;
}

static void visit_static_assert_declaration(struct visit_ctx* ctx, struct static_assert_declaration* p_static_assert_declaration)
{
    visit_expression(ctx, p_static_assert_declaration->constant_expression);

    if (ctx->target < LANGUAGE_C11)
    {
        /*let's hide everything first*/
        token_range_add_flag(p_static_assert_declaration->first_token,
            p_static_assert_declaration->last_token,
            TK_C_BACKEND_FLAG_HIDE);
    }
    else if (ctx->target == LANGUAGE_C11)
    {
        if (p_static_assert_declaration->string_literal_opt == NULL)
        {
            struct token* _Opt rp = previous_parser_token(p_static_assert_declaration->last_token);

            if (rp != NULL)
                rp = previous_parser_token(rp);

            struct tokenizer_ctx tctx = { 0 };
            struct token_list list1 = tokenizer(&tctx, ", \"error\"", "", 0, TK_FLAG_FINAL);
            token_list_insert_after(&ctx->ast.token_list, rp, &list1);
            token_list_destroy(&list1);
        }
        if (strcmp(p_static_assert_declaration->first_token->lexeme, "static_assert") == 0)
        {
            char* _Owner _Opt temp = strdup("_Static_assert");
            if (temp == NULL)
                return;

            /*C23 has static_assert but C11 _Static_assert*/
            free(p_static_assert_declaration->first_token->lexeme);
            p_static_assert_declaration->first_token->lexeme = temp;
        }
    }
    else
    {
        char* _Owner _Opt temp = strdup("static_assert");
        if (temp == NULL)
            return;

        free(p_static_assert_declaration->first_token->lexeme);
        p_static_assert_declaration->first_token->lexeme = temp;
    }
}


static void visit_direct_declarator(struct visit_ctx* ctx, struct direct_declarator* p_direct_declarator)
{
    if (p_direct_declarator->function_declarator)
    {
        struct parameter_declaration* _Opt parameter = NULL;

        if (p_direct_declarator->function_declarator->parameter_type_list_opt &&
            p_direct_declarator->function_declarator->parameter_type_list_opt->parameter_list)
        {
            parameter = p_direct_declarator->function_declarator->parameter_type_list_opt->parameter_list->head;
        }

        while (parameter)
        {
            if (parameter->attribute_specifier_sequence_opt)
            {
                visit_attribute_specifier_sequence(ctx, parameter->attribute_specifier_sequence_opt);
            }

            visit_declaration_specifiers(ctx, parameter->declaration_specifiers, &parameter->declarator->type);
            if (parameter->declarator)
            {
                visit_declarator(ctx, parameter->declarator);
            }

            parameter = parameter->next;
        }

    }
    else if (p_direct_declarator->array_declarator)
    {
        if (p_direct_declarator->array_declarator->assignment_expression)
        {
            visit_expression(ctx, p_direct_declarator->array_declarator->assignment_expression);
        }

        if (ctx->target < LANGUAGE_C99)
        {
            /*static and type qualifiers in parameter array declarators where added in C99*/
            if (p_direct_declarator->array_declarator->static_token_opt)
            {
                p_direct_declarator->array_declarator->static_token_opt->flags |= TK_C_BACKEND_FLAG_HIDE;

                if (p_direct_declarator->array_declarator->type_qualifier_list_opt)
                {
                    struct type_qualifier* _Opt p_type_qualifier =
                        p_direct_declarator->array_declarator->type_qualifier_list_opt->head;

                    while (p_type_qualifier)
                    {
                        p_type_qualifier->token->flags |= TK_C_BACKEND_FLAG_HIDE;
                        p_type_qualifier = p_type_qualifier->next;
                    }
                }
            }
        }
    }
}

static void visit_declarator(struct visit_ctx* ctx, struct declarator* p_declarator)
{
    bool need_transformation = false;

    if (p_declarator->pointer)
    {
        struct pointer* _Opt p = p_declarator->pointer;
        while (p)
        {
            if (p->type_qualifier_list_opt)
            {
                struct type_qualifier* _Opt current = p->type_qualifier_list_opt->head;
                while (current)
                {
                    visit_type_qualifier(ctx, current);
                    current = current->next;
                }
            }
            p = p->pointer;
        }
    }

    if (ctx->target < LANGUAGE_C23)
    {
        if (p_declarator->declaration_specifiers)
        {
            if (p_declarator->declaration_specifiers->storage_class_specifier_flags & STORAGE_SPECIFIER_AUTO)
            {
                need_transformation = true;
            }
            if (p_declarator->declaration_specifiers->type_specifier_flags & TYPE_SPECIFIER_TYPEOF)
            {
                need_transformation = true;
            }
        }

        if (p_declarator->specifier_qualifier_list &&
            p_declarator->specifier_qualifier_list->type_specifier_flags & TYPE_SPECIFIER_TYPEOF)
        {
            need_transformation = true;
        }
    }


    //we may have a diference type from the current syntax 
    if (need_transformation)
    {

        struct osstream ss = { 0 };

        /*types like nullptr are converted to other types like void* */
        struct type new_type = type_convert_to(&p_declarator->type, ctx->target);

        type_remove_names(&new_type);
        if (p_declarator->name_opt)
        {
            free((void* _Owner)new_type.name_opt);
            new_type.name_opt = strdup(p_declarator->name_opt->lexeme);
        }

        print_type_declarator(&ss, &new_type);

        if (ss.c_str != NULL)
        {
            const int level = p_declarator->first_token_opt ? p_declarator->first_token_opt->level : 0;
            struct tokenizer_ctx tctx = { 0 };
            struct token_list l2 = tokenizer(&tctx, ss.c_str, NULL, level, TK_FLAG_FINAL);
            if (l2.head == NULL)
            {
                ss_close(&ss);
                type_destroy(&new_type);
                return;
            }

            /*let's hide the old declarator*/
              /*let's hide the old declarator*/
            if (p_declarator->first_token_opt != NULL &&
                p_declarator->last_token_opt != NULL &&
                p_declarator->first_token_opt != p_declarator->last_token_opt)
            {
                l2.head->flags = p_declarator->first_token_opt->flags;
                token_list_insert_after(&ctx->ast.token_list, p_declarator->last_token_opt, &l2);
                token_range_add_flag(p_declarator->first_token_opt, p_declarator->last_token_opt, TK_C_BACKEND_FLAG_HIDE);
            }
            else
            {

                if (p_declarator->first_token_opt == NULL &&
                    p_declarator->last_token_opt != NULL)
                {
                    l2.head->flags = p_declarator->last_token_opt->flags;
                    /*it is a empty declarator, so first_token is not part of declarator it only marks de position*/
                    token_list_insert_after(&ctx->ast.token_list, p_declarator->last_token_opt->prev, &l2);
                }
                else if (p_declarator->first_token_opt != NULL &&
                         p_declarator->last_token_opt != NULL)
                {
                    l2.head->flags = p_declarator->first_token_opt->flags;
                    /*it is a empty declarator, so first_token is not part of declarator it only marks de position*/
                    token_list_insert_after(&ctx->ast.token_list, p_declarator->last_token_opt, &l2);
                    token_range_add_flag(p_declarator->first_token_opt, p_declarator->last_token_opt, TK_C_BACKEND_FLAG_HIDE);
                }

            }
            token_list_destroy(&l2);
        }

        type_destroy(&new_type);
        ss_close(&ss);
    }


    if (p_declarator->direct_declarator)
    {
        visit_direct_declarator(ctx, p_declarator->direct_declarator);
    }
}

static void visit_init_declarator_list(struct visit_ctx* ctx, struct init_declarator_list* p_init_declarator_list)
{
    struct init_declarator* _Opt p_init_declarator = p_init_declarator_list->head;

    while (p_init_declarator)
    {

        visit_declarator(ctx, p_init_declarator->p_declarator);

        if (p_init_declarator->initializer)
        {
            if (p_init_declarator->initializer->assignment_expression)
            {
                visit_expression(ctx, p_init_declarator->initializer->assignment_expression);
            }
            else
            {
                if (p_init_declarator->initializer->braced_initializer)
                {
                    visit_bracket_initializer_list(ctx,
                        p_init_declarator->initializer->braced_initializer);
                }

            }
        }

        p_init_declarator = p_init_declarator->next;
    }
}



static void visit_member_declarator(struct visit_ctx* ctx, struct member_declarator* p_member_declarator)
{
    if (p_member_declarator->declarator)
    {
        visit_declarator(ctx, p_member_declarator->declarator);
    }
}

static void visit_member_declarator_list(struct visit_ctx* ctx, struct member_declarator_list* p_member_declarator_list)
{
    struct member_declarator* _Opt p_member_declarator = p_member_declarator_list->head;
    while (p_member_declarator)
    {
        visit_member_declarator(ctx, p_member_declarator);
        p_member_declarator = p_member_declarator->next;
    }
}
static void visit_member_declaration(struct visit_ctx* ctx, struct member_declaration* p_member_declaration)
{
    if (p_member_declaration->member_declarator_list_opt)
    {
        visit_specifier_qualifier_list(ctx,
            p_member_declaration->specifier_qualifier_list,
            &p_member_declaration->member_declarator_list_opt->head->declarator->type); /*se nao tem?*/
    }

    if (p_member_declaration->member_declarator_list_opt)
    {
        visit_member_declarator_list(ctx, p_member_declaration->member_declarator_list_opt);
    }
}

static void visit_member_declaration_list(struct visit_ctx* ctx, struct member_declaration_list* p_member_declaration_list)
{
    struct member_declaration* _Opt p_member_declaration = p_member_declaration_list->head;
    while (p_member_declaration)
    {
        visit_member_declaration(ctx, p_member_declaration);
        p_member_declaration = p_member_declaration->next;
    }
}

static void visit_attribute_specifier(struct visit_ctx* ctx, struct attribute_specifier* p_attribute_specifier)
{
    if (ctx->target < LANGUAGE_C23)
    {
        token_range_add_flag(p_attribute_specifier->first_token, p_attribute_specifier->last_token, TK_C_BACKEND_FLAG_HIDE);
    }
}

static void visit_attribute_specifier_sequence(struct visit_ctx* ctx, struct attribute_specifier_sequence* p_visit_attribute_specifier_sequence)
{
    struct attribute_specifier* _Opt current = p_visit_attribute_specifier_sequence->head;
    while (current)
    {
        visit_attribute_specifier(ctx, current);
        current = current->next;
    }
}

static void visit_struct_or_union_specifier(struct visit_ctx* ctx, struct struct_or_union_specifier* p_struct_or_union_specifier)
{

    if (p_struct_or_union_specifier->attribute_specifier_sequence_opt)
        visit_attribute_specifier_sequence(ctx, p_struct_or_union_specifier->attribute_specifier_sequence_opt);

    struct struct_or_union_specifier* _Opt p_complete = get_complete_struct_or_union_specifier(p_struct_or_union_specifier);

    if (p_struct_or_union_specifier->show_anonymous_tag && !ctx->is_second_pass)
    {
        struct token* first = p_struct_or_union_specifier->first_token;

        const char* tag = p_struct_or_union_specifier->tag_name;
        char buffer[sizeof(p_struct_or_union_specifier->tag_name) + 8] = { 0 };
        snprintf(buffer, sizeof buffer, " %s", tag);
        struct tokenizer_ctx tctx = { 0 };
        struct token_list l2 = tokenizer(&tctx, buffer, NULL, 0, TK_FLAG_FINAL);
        token_list_insert_after(&ctx->ast.token_list, first, &l2);
        token_list_destroy(&l2);
    }

    if (p_complete)
    {
        if (ctx->is_inside_lambda && !ctx->is_second_pass)
        {
            /*
              Na primeira passada marcamos os tipos que são renomeados
            */
            if (p_complete->scope_level >
                p_struct_or_union_specifier->scope_level &&
                p_complete->visit_moved == 0)
            {
                char newtag[212] = { 0 };
                snprintf(newtag, sizeof newtag, "_%s%d", p_struct_or_union_specifier->tag_name, ctx->capture_index);
                ctx->capture_index++;

                char* _Opt _Owner temp = strdup(newtag);
                if (temp == NULL)
                    return;

                if (p_complete->tagtoken != NULL)
                {
                    free(p_complete->tagtoken->lexeme);
                    p_complete->tagtoken->lexeme = temp;
                }
                else
                {
                    free(temp);
                    assert(false);
                }
                p_complete->visit_moved = 1;
            }
        }
        else if (ctx->is_second_pass)
        {
            /*
             Na segunda passada vou renomear quem usa este tag exporado
            */
            if (p_complete->visit_moved == 1)
            {
                if (p_struct_or_union_specifier != p_complete &&
                    p_complete->tagtoken != NULL &&
                    p_struct_or_union_specifier->tagtoken != NULL)
                {
                    char* _Opt _Owner temp = strdup(p_complete->tagtoken->lexeme);
                    if (temp == NULL)
                        return;

                    free(p_struct_or_union_specifier->tagtoken->lexeme);
                    p_struct_or_union_specifier->tagtoken->lexeme = temp;
                }
            }
        }
    }



    visit_member_declaration_list(ctx, &p_struct_or_union_specifier->member_declaration_list);

}

static void visit_enumerator(struct visit_ctx* ctx, struct enumerator* p_enumerator)
{
    if (p_enumerator->constant_expression_opt)
        visit_expression(ctx, p_enumerator->constant_expression_opt);

}

//struct enumerator_list* enumerator_list;
static void visit_enumerator_list(struct visit_ctx* ctx, struct enumerator_list* p_enumerator_list)
{
    struct enumerator* _Opt current = p_enumerator_list->head;
    while (current)
    {
        visit_enumerator(ctx, current);
        current = current->next;
    }
}

static void visit_enum_specifier(struct visit_ctx* ctx, struct enum_specifier* p_enum_specifier)
{
    if (ctx->target < LANGUAGE_C23)
    {
        if (p_enum_specifier->specifier_qualifier_list)
        {
            struct token* _Opt tk = p_enum_specifier->specifier_qualifier_list->first_token;
            while (tk)
            {
                if (tk->type == ':')
                    break;
                tk = tk->prev;
            }

            if (tk == NULL)
            {
                //error
                return;
            }

            token_range_add_flag(tk,
                p_enum_specifier->specifier_qualifier_list->last_token,
                TK_C_BACKEND_FLAG_HIDE);
        }

        const struct enum_specifier* _Opt p_complete_enum_specifier =
            get_complete_enum_specifier(p_enum_specifier);

        if (p_complete_enum_specifier != NULL &&
            p_enum_specifier != p_complete_enum_specifier &&
            p_complete_enum_specifier->specifier_qualifier_list)
        {
            p_enum_specifier->first_token->flags |= TK_C_BACKEND_FLAG_HIDE;

            if (p_enum_specifier->tag_token)
                p_enum_specifier->tag_token->flags |= TK_C_BACKEND_FLAG_HIDE;

            struct osstream ss = { 0 };
            bool b_first = true;

            print_type_qualifier_flags(&ss, &b_first, p_complete_enum_specifier->specifier_qualifier_list->type_qualifier_flags);
            print_type_specifier_flags(&ss, &b_first, p_complete_enum_specifier->specifier_qualifier_list->type_specifier_flags);

            if (ss.c_str == NULL)
                return;

            struct tokenizer_ctx tctx = { 0 };
            struct token_list l2 = tokenizer(&tctx, ss.c_str, NULL, 0, TK_FLAG_NONE);

            token_list_insert_after(&ctx->ast.token_list,
                p_enum_specifier->tag_token,
                &l2);

            ss_close(&ss);
            token_list_destroy(&l2);
        }

    }

    if (p_enum_specifier->attribute_specifier_sequence_opt)
    {
        visit_attribute_specifier_sequence(ctx, p_enum_specifier->attribute_specifier_sequence_opt);
    }

    visit_enumerator_list(ctx, &p_enum_specifier->enumerator_list);
}

static void visit_typeof_specifier(struct visit_ctx* ctx, struct typeof_specifier* p_typeof_specifier)
{}

static void visit_type_specifier(struct visit_ctx* ctx, struct type_specifier* p_type_specifier)
{
    try
    {
        if (p_type_specifier->typeof_specifier)
        {
            visit_typeof_specifier(ctx, p_type_specifier->typeof_specifier);
        }

        if (p_type_specifier->struct_or_union_specifier)
        {
            visit_struct_or_union_specifier(ctx, p_type_specifier->struct_or_union_specifier);
        }

        if (p_type_specifier->enum_specifier)
        {
            visit_enum_specifier(ctx, p_type_specifier->enum_specifier);
        }


        if (p_type_specifier->atomic_type_specifier)
        {
            //visit_deped(ctx, p_type_specifier->enum_specifier);
        }

        if (p_type_specifier->flags & TYPE_SPECIFIER_BOOL)
        {
            if (ctx->target < LANGUAGE_C99)
            {
                char* _Owner _Opt temp = strdup("unsigned char");
                if (temp == NULL) throw;

                free(p_type_specifier->token->lexeme);
                p_type_specifier->token->lexeme = temp;
            }
            else
            {
                if (ctx->target < LANGUAGE_C23)
                {
                    if (strcmp(p_type_specifier->token->lexeme, "bool") == 0)
                    {
                        char* _Owner _Opt temp = strdup("_Bool");
                        if (temp == NULL) throw;

                        free(p_type_specifier->token->lexeme);
                        p_type_specifier->token->lexeme = temp;
                    }
                }
                else
                {
                    char* _Owner _Opt temp = strdup("bool");
                    if (temp == NULL) throw;

                    free(p_type_specifier->token->lexeme);
                    p_type_specifier->token->lexeme = temp;
                }
            }
        }
    }
    catch
    {
    }
}

static void visit_type_specifier_qualifier(struct visit_ctx* ctx, struct type_specifier_qualifier* p_type_specifier_qualifier)
{
    if (p_type_specifier_qualifier->type_qualifier)
    {
        visit_type_qualifier(ctx, p_type_specifier_qualifier->type_qualifier);
    }
    else if (p_type_specifier_qualifier->type_specifier)
    {
        visit_type_specifier(ctx, p_type_specifier_qualifier->type_specifier);
    }
    else if (p_type_specifier_qualifier->alignment_specifier)
    {
    }
}

static void visit_storage_class_specifier(struct visit_ctx* ctx, struct storage_class_specifier* p_storage_class_specifier)
{
    if (p_storage_class_specifier->flags & STORAGE_SPECIFIER_AUTO)
    {
        if (ctx->target < LANGUAGE_C23)
        {
            p_storage_class_specifier->token->flags |= TK_C_BACKEND_FLAG_HIDE;
        }
    }
}

static void visit_declaration_specifier(struct visit_ctx* ctx, struct declaration_specifier* p_declaration_specifier)
{
    if (p_declaration_specifier->function_specifier)
    {
        if (p_declaration_specifier->function_specifier->token->type == TK_KEYWORD__NORETURN)
        {
            if (ctx->target < LANGUAGE_C11)
            {
                char* _Opt _Owner temp = strdup("/*[[noreturn]]*/");
                if (temp == NULL)
                    return;

                free(p_declaration_specifier->function_specifier->token->lexeme);
                p_declaration_specifier->function_specifier->token->lexeme = temp;
            }
            else if (ctx->target == LANGUAGE_C11)
            {
                /*nothing*/
            }
            else if (ctx->target > LANGUAGE_C11)
            {
                char* _Opt _Owner temp = strdup("[[noreturn]]");
                if (temp == NULL)
                    return;

                /*use attributes*/
                free(p_declaration_specifier->function_specifier->token->lexeme);
                p_declaration_specifier->function_specifier->token->lexeme = temp;
            }

        }
    }


    if (p_declaration_specifier->storage_class_specifier)
    {
        visit_storage_class_specifier(ctx, p_declaration_specifier->storage_class_specifier);

    }

    if (p_declaration_specifier->type_specifier_qualifier)
    {
        visit_type_specifier_qualifier(ctx, p_declaration_specifier->type_specifier_qualifier);

    }

}

static void visit_declaration_specifiers(struct visit_ctx* ctx,
    struct declaration_specifiers* p_declaration_specifiers,
    struct type* _Opt p_type_opt)
{
    /*
        * Se tiver typeof ou auto vamos apagar todos type specifiers.
        * e trocar por um novo
        * const typeof(int (*)()) a;
           //a = 1;
          auto p = (const typeof(int (*)())) 0;

          TODO esconder os type spefiver e qualifider , esconder auto.
          o resto tipo static deixar.

        */
        //
    if (!ctx->is_second_pass &&
        ctx->target < LANGUAGE_C23 &&
        (p_declaration_specifiers->storage_class_specifier_flags & STORAGE_SPECIFIER_AUTO ||
            p_declaration_specifiers->type_specifier_flags & TYPE_SPECIFIER_TYPEOF))
    {

        struct declaration_specifier* _Opt p_declaration_specifier = p_declaration_specifiers->head;
        while (p_declaration_specifier)
        {
            if (p_declaration_specifier->function_specifier)
            {
            }
            if (p_declaration_specifier->storage_class_specifier)
            {
            }
            if (p_declaration_specifier->type_specifier_qualifier)
            {
                if (p_declaration_specifier->type_specifier_qualifier->type_qualifier)
                {
                    p_declaration_specifier->type_specifier_qualifier->type_qualifier->token->flags |= TK_C_BACKEND_FLAG_HIDE;
                }
                if (p_declaration_specifier->type_specifier_qualifier->type_specifier)
                {
                    if (p_declaration_specifier->type_specifier_qualifier->type_specifier->typeof_specifier)
                    {
                        token_range_add_flag(p_declaration_specifier->type_specifier_qualifier->type_specifier->typeof_specifier->first_token,
                            p_declaration_specifier->type_specifier_qualifier->type_specifier->typeof_specifier->last_token,
                            TK_C_BACKEND_FLAG_HIDE);
                    }
                    p_declaration_specifier->type_specifier_qualifier->type_specifier->token->flags |= TK_C_BACKEND_FLAG_HIDE;
                }
            }
            p_declaration_specifier = p_declaration_specifier->next;
        }


        /*now we print new specifiers then convert to tokens*/
        struct osstream ss0 = { 0 };
        struct type new_type = { 0 };

        if (p_type_opt)
            new_type = type_convert_to(p_type_opt, ctx->target);

        const struct type* p = type_get_specifer_part(&new_type);
        print_type_qualifier_specifiers(&ss0, p);

        const int level = p_declaration_specifiers->last_token->level;
        struct tokenizer_ctx tctx = { 0 };

        if (ss0.c_str == NULL)
        {
            type_destroy(&new_type);
            return;
        }

        struct token_list l2 = tokenizer(&tctx, ss0.c_str, NULL, level, TK_FLAG_FINAL);

        token_list_insert_after(&ctx->ast.token_list, p_declaration_specifiers->last_token, &l2);


        type_destroy(&new_type);
        ss_close(&ss0);
        token_list_destroy(&l2);
    }

    struct declaration_specifier* _Opt p_declaration_specifier = p_declaration_specifiers->head;

    struct declaration_specifier* _Opt p_constexpr_declaration_specifier = NULL;
    while (p_declaration_specifier)
    {
        if (p_declaration_specifier->storage_class_specifier &&
            p_declaration_specifier->storage_class_specifier->flags & STORAGE_SPECIFIER_CONSTEXPR)
        {
            p_constexpr_declaration_specifier = p_declaration_specifier;
        }

        visit_declaration_specifier(ctx, p_declaration_specifier);
        p_declaration_specifier = p_declaration_specifier->next;
    }


    if (ctx->target < LANGUAGE_C23)
    {
        /*
          fixing constexpr, we add static const if necessary
        */
        if (p_constexpr_declaration_specifier &&
            p_declaration_specifiers->storage_class_specifier_flags & STORAGE_SPECIFIER_CONSTEXPR)
        {
            struct osstream ss = { 0 };
            const bool is_file_scope =
                p_declaration_specifiers->storage_class_specifier_flags & STORAGE_SPECIFIER_CONSTEXPR_STATIC;

            const bool has_static =
                p_declaration_specifiers->storage_class_specifier_flags & STORAGE_SPECIFIER_STATIC;

            const bool has_const =
                p_declaration_specifiers->type_qualifier_flags & TYPE_QUALIFIER_CONST;


            if (is_file_scope && !has_static)
            {
                ss_fprintf(&ss, "static");
                if (!has_const)
                {
                    ss_fprintf(&ss, " const");
                }
            }
            else
            {
                if (!has_const)
                {
                    ss_fprintf(&ss, "const");
                }
                else
                {
                    ss_fprintf(&ss, " ");
                }
            }

            if (ss.c_str == NULL)
                return;

            assert(p_constexpr_declaration_specifier->storage_class_specifier != NULL);

            free(p_constexpr_declaration_specifier->storage_class_specifier->token->lexeme);
            p_constexpr_declaration_specifier->storage_class_specifier->token->lexeme = ss.c_str;
            ss.c_str = NULL; /*MOVED*/

            ss_close(&ss);
        }
    }

}

/*
* retorna true se o ultimo item for um return
*/
static bool is_last_item_return(struct compound_statement* p_compound_statement)
{
    if (/*p_compound_statement &&*/
        p_compound_statement->block_item_list.tail &&
        p_compound_statement->block_item_list.tail->unlabeled_statement &&
        p_compound_statement->block_item_list.tail->unlabeled_statement->jump_statement &&
        p_compound_statement->block_item_list.tail->unlabeled_statement->jump_statement->first_token->type == TK_KEYWORD_RETURN)
    {
        return true;
    }
    return false;
}

void defer_scope_delete_one(struct defer_scope* _Owner p_block)
{

    struct defer_scope* _Owner _Opt child = p_block->lastchild;
    while (child != NULL)
    {
        struct defer_scope* _Owner _Opt prev = child->previous;

        child->previous = NULL;
        defer_scope_delete_one(child);

        child = prev;
    }

    assert(p_block->previous == NULL);
    free(p_block);

}

void defer_scope_delete_all(struct defer_scope* _Owner p)
{
    struct defer_scope* _Owner _Opt p_block = p;
    while (p_block != NULL)
    {
        struct defer_scope* _Owner _Opt prev_block = p_block->previous;
        p_block->previous = NULL;
        defer_scope_delete_one(p_block);
        p_block = prev_block;
    }
}

static void visit_declaration(struct visit_ctx* ctx, struct declaration* p_declaration)
{
    if (p_declaration->static_assert_declaration)
    {
        visit_static_assert_declaration(ctx, p_declaration->static_assert_declaration);
    }

    if (p_declaration->pragma_declaration)
    {
        visit_pragma_declaration(ctx, p_declaration->pragma_declaration);
    }

    if (p_declaration->p_attribute_specifier_sequence_opt)
    {
        visit_attribute_specifier_sequence(ctx, p_declaration->p_attribute_specifier_sequence_opt);
    }

    if (p_declaration->declaration_specifiers)
    {
        if (p_declaration->init_declarator_list.head)
        {
            visit_declaration_specifiers(ctx, p_declaration->declaration_specifiers,
                &p_declaration->init_declarator_list.head->p_declarator->type);
        }
        else
        {
            visit_declaration_specifiers(ctx, p_declaration->declaration_specifiers, NULL);
        }

    }

    if (p_declaration->p_attribute_specifier_sequence_opt)
    {
        if (!ctx->is_second_pass)
        {
            token_range_add_flag(p_declaration->p_attribute_specifier_sequence_opt->first_token,
                p_declaration->p_attribute_specifier_sequence_opt->last_token,
                TK_C_BACKEND_FLAG_HIDE);

        }
    }
    if (ctx->is_second_pass)
    {

        if (p_declaration->declaration_specifiers &&
            p_declaration->declaration_specifiers->type_specifier_flags == TYPE_SPECIFIER_STRUCT_OR_UNION)
        {
            assert(p_declaration->declaration_specifiers->struct_or_union_specifier != NULL);
            if (p_declaration->declaration_specifiers->struct_or_union_specifier->tagtoken == NULL)
            {
                assert(false);
                return;
            }

            if (p_declaration->declaration_specifiers->struct_or_union_specifier->visit_moved == 1)
            {
                struct tokenizer_ctx tctx = { 0 };
                struct token_list list0 = tokenizer(&tctx, "struct ", NULL, 0, TK_FLAG_FINAL);
                token_list_append_list(&ctx->insert_before_declaration, &list0);
                token_list_destroy(&list0);


                struct token_list list = tokenizer(&tctx, p_declaration->declaration_specifiers->struct_or_union_specifier->tagtoken->lexeme, NULL, 0, TK_FLAG_FINAL);
                token_list_append_list(&ctx->insert_before_declaration, &list);
                token_list_destroy(&list);

                //struct token_list list3 = tokenizer("{", NULL, 0, TK_FLAG_FINAL);
                //token_list_append_list(&ctx->insert_before_declaration, &list3);



                struct token* first = p_declaration->declaration_specifiers->struct_or_union_specifier->member_declaration_list.first_token;
                struct token* last = p_declaration->declaration_specifiers->struct_or_union_specifier->member_declaration_list.last_token;
                for (struct token* current = first;
                    current != last->next;
                    current = current->next)
                {
                    token_list_clone_and_add(&ctx->insert_before_declaration, current);
                    //current->flags |= TK_FLAG_FINAL;
                    if (current->next == NULL)
                        break;
                }

                struct token_list list3 = tokenizer(&tctx, ";\n", NULL, 0, TK_FLAG_FINAL);
                token_list_append_list(&ctx->insert_before_declaration, &list3);


                if (p_declaration->init_declarator_list.head == NULL)
                {
                    token_range_add_flag(p_declaration->declaration_specifiers->struct_or_union_specifier->first_token,
                        p_declaration->declaration_specifiers->struct_or_union_specifier->last_token,
                        TK_C_BACKEND_FLAG_HIDE);
                }
                else
                {
                    token_range_add_flag(p_declaration->declaration_specifiers->struct_or_union_specifier->member_declaration_list.first_token,
                        p_declaration->declaration_specifiers->struct_or_union_specifier->member_declaration_list.last_token,
                        TK_C_BACKEND_FLAG_HIDE);
                }
                token_list_destroy(&list3);
            }
        }
    }


    if (p_declaration->init_declarator_list.head)
    {
        visit_init_declarator_list(ctx, &p_declaration->init_declarator_list);
    }

    if (p_declaration->function_body)
    {
        ctx->has_lambda = false;
        ctx->is_second_pass = false;


        struct defer_scope* _Opt p_defer = visit_ctx_push_tail_block(ctx);
        if (p_defer == NULL)
            return;

        p_defer->p_function_body = p_declaration->function_body;

        visit_compound_statement(ctx, p_declaration->function_body);

        if (!is_last_item_return(p_declaration->function_body))
        {
            struct osstream ss = { 0 };
            print_block_defer(p_defer, &ss, true);

            if (ss.size > 0)
            {
                assert(ss.c_str != NULL);
                struct tokenizer_ctx tctx = { 0 };
                struct token_list l = tokenizer(&tctx, ss.c_str, NULL, 0, TK_FLAG_FINAL);
                token_list_insert_after(&ctx->ast.token_list, p_declaration->function_body->last_token->prev, &l);
                token_list_destroy(&l);
            }
            ss_close(&ss);
        }
        else
        {
            //ja tem um return antes que chama defer
            hide_block_defer(p_defer);
        }

        visit_ctx_pop_tail_block(ctx);



        if (ctx->has_lambda)
        {
            /*functions with lambdas requires two phases*/
            ctx->is_second_pass = true;
            visit_compound_statement(ctx, p_declaration->function_body);
        }
    }


    /*
       In direct mode, we hide non used declarations (just to make the result smaller)
    */
    if (ctx->hide_non_used_declarations &&
        p_declaration->init_declarator_list.head)
    {
        if (p_declaration->init_declarator_list.head->p_declarator->num_uses == 0 &&
            p_declaration->init_declarator_list.head->p_declarator->function_body == NULL)
        {
            /*
              transformations must keep first_token and last_token correct - updated
            */
            token_range_add_flag(p_declaration->first_token, p_declaration->last_token, TK_C_BACKEND_FLAG_HIDE);
        }
    }
}

int visit_literal_string(struct visit_ctx* ctx, struct token* current)
{
    try
    {
        bool has_u8_prefix =
            current->lexeme[0] == 'u' && current->lexeme[1] == '8';

        if (has_u8_prefix && ctx->target < LANGUAGE_C11)
        {
            struct osstream ss = { 0 };
            unsigned char* psz = (unsigned char*)(current->lexeme + 2);

            while (*psz)
            {
                if (*psz >= 128)
                {
                    ss_fprintf(&ss, "\\x%x", *psz);
                }
                else
                {
                    ss_fprintf(&ss, "%c", *psz);
                }
                psz++;
            }

            if (ss.c_str == NULL)
            {
                throw;
            }

            free(current->lexeme);
            current->lexeme = ss.c_str;
            ss.c_str = NULL;
            ss_close(&ss);
        }
    }
    catch
    {
    }

    return 0;
}

int visit_tokens(struct visit_ctx* ctx)
{
    try
    {
        struct token* _Opt current = ctx->ast.token_list.head;
        while (current)
        {

            if (current->type == TK_STRING_LITERAL)
            {
                //C99 u8 prefix
                visit_literal_string(ctx, current);

                current = current->next;
                continue;
            }

            if (ctx->target < LANGUAGE_C99 && current->type == TK_LINE_COMMENT)
            {
                struct osstream ss = { 0 };
                //TODO  check /* inside
                ss_fprintf(&ss, "/*%s*/", current->lexeme + 2);
                if (ss.c_str == NULL)
                {
                    throw;
                }

                free(current->lexeme);
                current->lexeme = ss.c_str;

                current = current->next;
                continue;
            }

            if (current->type == TK_CHAR_CONSTANT)
            {
                if (ctx->target < LANGUAGE_C23 && current->lexeme[0] == 'u' && current->lexeme[1] == '8')
                {
                    char buffer[25] = { 0 };
                    snprintf(buffer, sizeof buffer, "((unsigned char)%s)", current->lexeme + 2);
                    char* _Owner _Opt newlexeme = strdup(buffer);
                    if (newlexeme)
                    {
                        free(current->lexeme);
                        current->lexeme = newlexeme;
                    }
                    current = current->next;
                    continue;
                }

                if (ctx->target < LANGUAGE_C11 && current->lexeme[0] == 'u' && current->lexeme[1] == '\'')
                {
                    const unsigned char* _Opt s = (const unsigned char*)(current->lexeme + 2);
                    unsigned int c;
                    s = utf8_decode(s, &c);

                    char buffer[25] = { 0 };
                    snprintf(buffer, sizeof buffer, "((unsigned short)%d)", c);
                    char* _Opt _Owner newlexeme = strdup(buffer);
                    if (newlexeme)
                    {
                        free(current->lexeme);
                        current->lexeme = newlexeme;
                    }
                    current = current->next;
                    continue;
                }

                if (ctx->target < LANGUAGE_C11 && current->lexeme[0] == 'U' && current->lexeme[1] == '\'')
                {
                    const unsigned char* _Opt s = (const unsigned char*)current->lexeme + 2;
                    unsigned int c;
                    s = utf8_decode(s, &c);

                    char buffer[25] = { 0 };
                    snprintf(buffer, sizeof buffer, "%du", c);
                    char* _Owner _Opt newlexeme = strdup(buffer);
                    if (newlexeme)
                    {
                        free(current->lexeme);
                        current->lexeme = newlexeme;
                    }
                    current = current->next;
                    continue;
                }

            }

            if (current->type == TK_COMPILER_DECIMAL_CONSTANT ||
                current->type == TK_COMPILER_OCTAL_CONSTANT ||
                current->type == TK_COMPILER_HEXADECIMAL_CONSTANT ||
                current->type == TK_COMPILER_DECIMAL_FLOATING_CONSTANT ||
                current->type == TK_PPNUMBER ||
                current->type == TK_COMPILER_HEXADECIMAL_FLOATING_CONSTANT)
            {
                if (ctx->target < LANGUAGE_C23)
                {
                    /*remove C23 digit separators*/
                    remove_char(current->lexeme, '\'');
                }

                if (ctx->target < LANGUAGE_C99 && current->type == TK_COMPILER_HEXADECIMAL_FLOATING_CONSTANT)
                {
                    /*
                     * C99 Hexadecimal floating constants to C89.
                     */
                    long double d = strtold(current->lexeme, NULL);
                    char buffer[50] = { 0 };
                    snprintf(buffer, sizeof buffer, "%Lg", d);

                    char* _Owner _Opt temp = strdup(buffer);
                    if (temp == NULL)
                        throw;

                    free(current->lexeme);
                    current->lexeme = temp;
                }

                if (ctx->target < LANGUAGE_C2Y && current->type == TK_COMPILER_OCTAL_CONSTANT)
                {
                    if (current->lexeme[1] == 'o' || current->lexeme[1] == 'O')
                    {
                        //We remove the prefix O o
                        //C2Y
                        //https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3319.htm

                        char buffer[50] = { 0 };
                        snprintf(buffer, sizeof buffer, "0%s", current->lexeme + 2);

                        char* _Owner _Opt temp = strdup(buffer);
                        if (temp == NULL)
                            throw;

                        free(current->lexeme);
                        current->lexeme = temp;
                    }
                }

                current = current->next;
                continue;
            }


            if ((current->type == TK_COMPILER_BINARY_CONSTANT) ||
                      (current->type == TK_PPNUMBER && current->lexeme[0] == '0' &&
                          (current->lexeme[1] == 'b' || current->lexeme[1] == 'B')) /*dentro macros*/
                      )
            {
                if (ctx->target < LANGUAGE_C23)
                {
                    /*remove C23 digit separators*/
                    remove_char(current->lexeme, '\'');
                }

                if (ctx->target < LANGUAGE_C23)
                {
                    /*
                    * Convert C23 binary literals to C99 hex
                    */
                    current->type = TK_COMPILER_HEXADECIMAL_CONSTANT;
                    int value = strtol(current->lexeme + 2, NULL, 2);
                    char buffer[33 + 2] = { '0', 'x' };
                    snprintf(buffer, sizeof buffer, "0x%x", value);

                    char* _Opt _Owner p_temp = strdup(buffer);
                    if (p_temp == NULL)
                    {
                        throw;
                    }

                    free(current->lexeme);
                    current->lexeme = p_temp;
                }

                current = current->next;
                continue;
            }


            if (current->type == TK_PREPROCESSOR_LINE)
            {
                struct token* first_preprocessor_token = current;
                struct token* _Opt last_preprocessor_token = current;

                while (last_preprocessor_token)
                {
                    if (last_preprocessor_token->next == NULL ||
                        last_preprocessor_token->next->type == TK_NEWLINE ||
                        last_preprocessor_token->next->type == TK_PRAGMA_END)
                    {
                        break;
                    }
                    last_preprocessor_token = last_preprocessor_token->next;
                }

                current = current->next;

                while (current && current->type == TK_BLANKS)
                {
                    current = current->next;
                }

                if (current == NULL) break;

                if (strcmp(current->lexeme, "pragma") == 0)
                {
                    current = current->next;

                    /*skip blanks*/
                    while (current && current->type == TK_BLANKS)
                    {
                        current = current->next;
                    }

                    if (current == NULL) break;

                    if (strcmp(current->lexeme, "safety") == 0 ||
                        strcmp(current->lexeme, "nullable") == 0 ||
                        strcmp(current->lexeme, "expand") == 0 ||
                        strcmp(current->lexeme, "flow") == 0)
                    {
                        del(first_preprocessor_token, last_preprocessor_token);

                        current = current->next;
                        continue;
                    }
                }

                if (ctx->target < LANGUAGE_C23 &&
                    strcmp(current->lexeme, "warning") == 0)
                {
                    /*
                      change C23 #warning to comment
                    */
                    free(first_preprocessor_token->lexeme);
                    char* _Opt _Owner temp = strdup("//#");
                    if (temp == NULL)
                    {
                        throw;
                    }

                    first_preprocessor_token->lexeme = temp;

                    current = current->next;
                    continue;
                }

                if (ctx->target < LANGUAGE_C23 &&
                    strcmp(current->lexeme, "elifdef") == 0)
                {
                    /*
                      change C23 #elifdef to #elif defined e #elifndef to C11
                    */
                    free(current->lexeme);
                    char* _Opt _Owner temp = strdup("elif defined ");
                    if (temp == NULL)
                    {
                        throw;
                    }

                    current->lexeme = temp;
                    current = current->next;
                    continue;
                }

                if (ctx->target < LANGUAGE_C23 &&
                    strcmp(current->lexeme, "elifndef") == 0)
                {
                    /*
                     change C23 #elifndef to #elif !defined
                    */

                    free(current->lexeme);
                    char* _Owner _Opt temp = strdup("elif ! defined ");
                    if (temp == NULL)
                    {
                        throw;
                    }

                    current->lexeme = temp;

                    current = current->next;
                    continue;
                }
            }

            current = current->next;
        }
    }
    catch
    {
    }

    return 0;
}

void visit(struct visit_ctx* ctx)
{
    ctx->capture_index = 0;
    ctx->lambdas_index = 0;

    if (visit_tokens(ctx) != 0)
        return;

    struct declaration* _Opt p_declaration = ctx->ast.declaration_list.head;
    while (p_declaration)
    {
        visit_declaration(ctx, p_declaration);

        if (ctx->insert_before_block_item.head != NULL)
        {
            if (p_declaration->first_token->prev)
            {
                token_list_insert_after(&ctx->ast.token_list, p_declaration->first_token->prev, &ctx->insert_before_block_item);
            }
        }

        /*
        * Tem que inserir algo antes desta declaracao?
        */
        if (ctx->insert_before_declaration.head != NULL)
        {
            if (p_declaration->first_token->prev)
            {
                token_list_insert_after(&ctx->ast.token_list, p_declaration->first_token->prev, &ctx->insert_before_declaration);
            }

        }

        p_declaration = p_declaration->next;
    }
    //if (ctx->instanciations.head != NULL)
    //{
    //    token_list_append_list(&ctx->ast.token_list, &ctx->instanciations);
    //}
}

