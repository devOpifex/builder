#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Rinternals.h>
#include <R_ext/Parse.h>

#include "s7check.h"
#include "log.h"

static const char *KNOWN_CTORS[] = {
  "class_character", "class_numeric", "class_double",
  "class_integer", "class_logical", "class_function",
  "class_list", "class_complex", "class_raw",
  "class_factor", "class_data.frame", "class_atomic",
  "class_any", "class_vector", "class_environment",
  "class_Date", "class_POSIXct", "class_formula",
  "class_name", "class_call", "class_expression",
  NULL
};

static int call_head_matches(SEXP head, const char *fn, const char *pkg)
{
  if (TYPEOF(head) == SYMSXP) {
    return strcmp(CHAR(PRINTNAME(head)), fn) == 0;
  }
  if (pkg != NULL && TYPEOF(head) == LANGSXP) {
    SEXP op = CAR(head);
    if (TYPEOF(op) == SYMSXP &&
        (strcmp(CHAR(PRINTNAME(op)), "::") == 0 ||
         strcmp(CHAR(PRINTNAME(op)), ":::") == 0)) {
      SEXP pkg_sym = CADR(head);
      SEXP fn_sym = CADDR(head);
      if (TYPEOF(pkg_sym) == SYMSXP && TYPEOF(fn_sym) == SYMSXP &&
          strcmp(CHAR(PRINTNAME(pkg_sym)), pkg) == 0 &&
          strcmp(CHAR(PRINTNAME(fn_sym)), fn) == 0) {
        return 1;
      }
    }
  }
  return 0;
}

static const char *type_ctor_of(SEXP expr)
{
  if (TYPEOF(expr) != LANGSXP) return NULL;
  SEXP head = CAR(expr);
  for (int i = 0; KNOWN_CTORS[i] != NULL; i++) {
    if (call_head_matches(head, KNOWN_CTORS[i], "S7")) return KNOWN_CTORS[i];
  }
  return NULL;
}

// 1 = match, 0 = clear mismatch (warn), -1 = unknown (skip)
static int literal_matches_ctor(SEXP value, const char *ctor)
{
  int t = TYPEOF(value);

  if (strcmp(ctor, "class_character") == 0) {
    if (t == STRSXP) return 1;
    if (t == LGLSXP || t == REALSXP || t == INTSXP) return 0;
    return -1;
  }

  if (strcmp(ctor, "class_numeric") == 0 ||
      strcmp(ctor, "class_double") == 0) {
    if (t == REALSXP || t == INTSXP) return 1;
    if (t == STRSXP || t == LGLSXP) return 0;
    return -1;
  }

  if (strcmp(ctor, "class_integer") == 0) {
    if (t == INTSXP) return 1;
    if (t == STRSXP || t == LGLSXP) return 0;
    // bare numeric literal like 1 is REALSXP — could be valid integer; skip
    return -1;
  }

  if (strcmp(ctor, "class_logical") == 0) {
    if (t == LGLSXP) return 1;
    if (t == STRSXP || t == REALSXP || t == INTSXP) return 0;
    return -1;
  }

  if (strcmp(ctor, "class_function") == 0) {
    if (t == LANGSXP) {
      SEXP h = CAR(value);
      if (TYPEOF(h) == SYMSXP &&
          strcmp(CHAR(PRINTNAME(h)), "function") == 0) return 1;
    }
    if (t == STRSXP || t == REALSXP || t == INTSXP || t == LGLSXP) return 0;
    return -1;
  }

  if (strcmp(ctor, "class_list") == 0) {
    if (t == LANGSXP) {
      SEXP h = CAR(value);
      if (TYPEOF(h) == SYMSXP &&
          strcmp(CHAR(PRINTNAME(h)), "list") == 0) return 1;
    }
    if (t == STRSXP || t == REALSXP || t == INTSXP || t == LGLSXP) return 0;
    return -1;
  }

  return -1;
}

static const char *sexp_kind_for_msg(SEXP value)
{
  switch (TYPEOF(value)) {
    case STRSXP: return "character";
    case INTSXP: return "integer";
    case REALSXP: return "numeric";
    case LGLSXP: return "logical";
    case NILSXP: return "NULL";
    case LANGSXP: {
      SEXP h = CAR(value);
      if (TYPEOF(h) == SYMSXP) {
        const char *nm = CHAR(PRINTNAME(h));
        if (strcmp(nm, "function") == 0) return "function";
        if (strcmp(nm, "list") == 0) return "list";
      }
      return "call";
    }
    case SYMSXP: return "symbol";
    default: return "expression";
  }
}

static S7Class* find_class(S7Class *list, const char *name)
{
  while (list != NULL) {
    if (strcmp(list->var_name, name) == 0) return list;
    list = list->next;
  }
  return NULL;
}

static S7Prop* find_prop(S7Class *all, S7Class *cls, const char *name)
{
  // Walk up the parent chain so inherited properties are visible.
  // Bound the depth to defend against accidental cycles in user code.
  for (int depth = 0; cls != NULL && depth < 64; depth++) {
    S7Prop *p = cls->props;
    while (p != NULL) {
      if (strcmp(p->name, name) == 0) return p;
      p = p->next;
    }
    if (cls->parent_name == NULL) return NULL;
    cls = find_class(all, cls->parent_name);
  }
  return NULL;
}

static S7Instance* find_instance(S7Instance *list, const char *name)
{
  while (list != NULL) {
    if (strcmp(list->var_name, name) == 0) return list;
    list = list->next;
  }
  return NULL;
}

static void free_classes(S7Class *cls)
{
  while (cls != NULL) {
    S7Class *next_cls = cls->next;
    S7Prop *p = cls->props;
    while (p != NULL) {
      S7Prop *next_p = p->next;
      free(p->name);
      free(p->type_ctor);
      free(p);
      p = next_p;
    }
    free(cls->var_name);
    free(cls->parent_name);
    free(cls);
    cls = next_cls;
  }
}

static void free_instances(S7Instance *ins)
{
  while (ins != NULL) {
    S7Instance *next = ins->next;
    free(ins->var_name);
    free(ins);
    ins = next;
  }
}

static int is_assign_op(SEXP head)
{
  if (TYPEOF(head) != SYMSXP) return 0;
  const char *op = CHAR(PRINTNAME(head));
  return strcmp(op, "<-") == 0 || strcmp(op, "=") == 0 ||
         strcmp(op, "<<-") == 0;
}

static void collect_class_def(SEXP expr, S7Class **classes)
{
  if (TYPEOF(expr) != LANGSXP) return;
  if (!is_assign_op(CAR(expr))) return;

  SEXP lhs = CADR(expr);
  SEXP rhs = CADDR(expr);
  if (TYPEOF(lhs) != SYMSXP) return;
  if (TYPEOF(rhs) != LANGSXP) return;
  if (!call_head_matches(CAR(rhs), "new_class", "S7")) return;

  // Find properties = list(...) and parent = ... arguments
  SEXP args = CDR(rhs);
  SEXP props_arg = R_NilValue;
  SEXP parent_arg = R_NilValue;
  while (args != R_NilValue) {
    SEXP tag = TAG(args);
    if (tag != R_NilValue && TYPEOF(tag) == SYMSXP) {
      const char *tag_name = CHAR(PRINTNAME(tag));
      if (strcmp(tag_name, "properties") == 0) {
        props_arg = CAR(args);
      } else if (strcmp(tag_name, "parent") == 0) {
        parent_arg = CAR(args);
      }
    }
    args = CDR(args);
  }

  if (props_arg == R_NilValue) return;
  if (TYPEOF(props_arg) != LANGSXP) return;
  if (!call_head_matches(CAR(props_arg), "list", "base")) return;

  const char *cls_name = CHAR(PRINTNAME(lhs));
  if (find_class(*classes, cls_name) != NULL) return;

  // Extract parent class name. Accepts a bare symbol (Animal) or a
  // namespaced reference (S7::S7_object); anything else is left NULL.
  const char *parent_name = NULL;
  if (parent_arg != R_NilValue) {
    if (TYPEOF(parent_arg) == SYMSXP) {
      parent_name = CHAR(PRINTNAME(parent_arg));
    } else if (TYPEOF(parent_arg) == LANGSXP) {
      SEXP op = CAR(parent_arg);
      if (TYPEOF(op) == SYMSXP &&
          (strcmp(CHAR(PRINTNAME(op)), "::") == 0 ||
           strcmp(CHAR(PRINTNAME(op)), ":::") == 0)) {
        SEXP fn_sym = CADDR(parent_arg);
        if (TYPEOF(fn_sym) == SYMSXP) {
          parent_name = CHAR(PRINTNAME(fn_sym));
        }
      }
    }
  }

  S7Class *cls = malloc(sizeof(S7Class));
  if (cls == NULL) return;
  cls->var_name = strdup(cls_name);
  cls->parent_name = parent_name ? strdup(parent_name) : NULL;
  cls->props = NULL;
  cls->next = *classes;
  *classes = cls;

  SEXP prop_args = CDR(props_arg);
  while (prop_args != R_NilValue) {
    SEXP tag = TAG(prop_args);
    SEXP val = CAR(prop_args);
    if (tag != R_NilValue && TYPEOF(tag) == SYMSXP) {
      S7Prop *p = malloc(sizeof(S7Prop));
      if (p != NULL) {
        p->name = strdup(CHAR(PRINTNAME(tag)));
        const char *ctor = type_ctor_of(val);
        p->type_ctor = ctor ? strdup(ctor) : NULL;
        p->next = cls->props;
        cls->props = p;
      }
    }
    prop_args = CDR(prop_args);
  }
}

static void walk_collect(SEXP expr, S7Class **classes)
{
  if (expr == R_NilValue) return;
  int t = TYPEOF(expr);

  if (t == LANGSXP) {
    collect_class_def(expr, classes);
    walk_collect(CAR(expr), classes);
    SEXP a = CDR(expr);
    while (a != R_NilValue) {
      walk_collect(CAR(a), classes);
      a = CDR(a);
    }
    return;
  }

  if (t == EXPRSXP || t == VECSXP) {
    R_xlen_t n = XLENGTH(expr);
    for (R_xlen_t i = 0; i < n; i++) {
      walk_collect(VECTOR_ELT(expr, i), classes);
    }
    return;
  }

  if (t == LISTSXP || t == DOTSXP) {
    while (expr != R_NilValue) {
      walk_collect(CAR(expr), classes);
      expr = CDR(expr);
    }
    return;
  }
}

static void check_call_against_class(SEXP call, S7Class *all, S7Class *cls,
                                     int line, const char *file)
{
  SEXP a = CDR(call);
  while (a != R_NilValue) {
    SEXP tag = TAG(a);
    SEXP val = CAR(a);
    if (tag != R_NilValue && TYPEOF(tag) == SYMSXP) {
      const char *prop_name = CHAR(PRINTNAME(tag));
      S7Prop *p = find_prop(all, cls, prop_name);
      if (p == NULL) {
        LOG_WARNING("Unknown S7 property '%s' for class '%s' - %s:%d",
                    prop_name, cls->var_name, file, line);
      } else if (p->type_ctor != NULL) {
        int m = literal_matches_ctor(val, p->type_ctor);
        if (m == 0) {
          LOG_WARNING(
            "S7 property '%s' of class '%s' expects %s but got %s - %s:%d",
            prop_name, cls->var_name, p->type_ctor,
            sexp_kind_for_msg(val), file, line);
        }
      }
    }
    a = CDR(a);
  }
}

static void check_at_access(SEXP at_call, S7Class *all, S7Instance *instances,
                            int line, const char *file, SEXP assigned_value)
{
  SEXP inst_sym = CADR(at_call);
  SEXP prop_sym = CADDR(at_call);
  if (TYPEOF(inst_sym) != SYMSXP || TYPEOF(prop_sym) != SYMSXP) return;

  const char *iv = CHAR(PRINTNAME(inst_sym));
  const char *pv = CHAR(PRINTNAME(prop_sym));

  S7Instance *ins = find_instance(instances, iv);
  if (ins == NULL) return;

  S7Prop *p = find_prop(all, ins->cls, pv);
  if (p == NULL) {
    LOG_WARNING("Unknown S7 property '%s' for class '%s' - %s:%d",
                pv, ins->cls->var_name, file, line);
    return;
  }

  if (assigned_value != R_NilValue && p->type_ctor != NULL) {
    int m = literal_matches_ctor(assigned_value, p->type_ctor);
    if (m == 0) {
      LOG_WARNING(
        "S7 property '%s' of class '%s' expects %s but got %s - %s:%d",
        pv, ins->cls->var_name, p->type_ctor,
        sexp_kind_for_msg(assigned_value), file, line);
    }
  }
}

static void walk_check(SEXP expr, S7Class *classes, S7Instance **instances,
                       int line, const char *file);

static void walk_args(SEXP args, S7Class *classes, S7Instance **instances,
                      int line, const char *file)
{
  while (args != R_NilValue) {
    walk_check(CAR(args), classes, instances, line, file);
    args = CDR(args);
  }
}

static void walk_check(SEXP expr, S7Class *classes, S7Instance **instances,
                       int line, const char *file)
{
  if (expr == R_NilValue) return;
  int t = TYPEOF(expr);

  if (t == LANGSXP) {
    SEXP head = CAR(expr);

    if (is_assign_op(head)) {
      SEXP lhs = CADR(expr);
      SEXP rhs = CADDR(expr);

      // instance@prop <- value
      if (TYPEOF(lhs) == LANGSXP) {
        SEXP lhs_head = CAR(lhs);
        if (TYPEOF(lhs_head) == SYMSXP &&
            strcmp(CHAR(PRINTNAME(lhs_head)), "@") == 0) {
          check_at_access(lhs, classes, *instances, line, file, rhs);
          walk_check(rhs, classes, instances, line, file);
          return;
        }
      }

      // instance <- ClsCall(...)
      if (TYPEOF(lhs) == SYMSXP && TYPEOF(rhs) == LANGSXP) {
        SEXP rhs_head = CAR(rhs);
        if (TYPEOF(rhs_head) == SYMSXP) {
          S7Class *cls = find_class(classes, CHAR(PRINTNAME(rhs_head)));
          if (cls != NULL) {
            const char *iv = CHAR(PRINTNAME(lhs));
            S7Instance *ni = malloc(sizeof(S7Instance));
            if (ni != NULL) {
              ni->var_name = strdup(iv);
              ni->cls = cls;
              ni->next = *instances;
              *instances = ni;
            }
            check_call_against_class(rhs, classes, cls, line, file);
            walk_args(CDR(rhs), classes, instances, line, file);
            return;
          }
        }
      }

      walk_check(lhs, classes, instances, line, file);
      walk_check(rhs, classes, instances, line, file);
      return;
    }

    if (TYPEOF(head) == SYMSXP &&
        strcmp(CHAR(PRINTNAME(head)), "@") == 0) {
      check_at_access(expr, classes, *instances, line, file, R_NilValue);
      return;
    }

    if (TYPEOF(head) == SYMSXP) {
      S7Class *cls = find_class(classes, CHAR(PRINTNAME(head)));
      if (cls != NULL) {
        check_call_against_class(expr, classes, cls, line, file);
        walk_args(CDR(expr), classes, instances, line, file);
        return;
      }
    }

    walk_check(head, classes, instances, line, file);
    walk_args(CDR(expr), classes, instances, line, file);
    return;
  }

  if (t == EXPRSXP || t == VECSXP) {
    R_xlen_t n = XLENGTH(expr);
    for (R_xlen_t i = 0; i < n; i++) {
      walk_check(VECTOR_ELT(expr, i), classes, instances, line, file);
    }
    return;
  }

  if (t == LISTSXP || t == DOTSXP) {
    while (expr != R_NilValue) {
      walk_check(CAR(expr), classes, instances, line, file);
      expr = CDR(expr);
    }
    return;
  }
}

static SEXP parse_code(const char *code)
{
  SEXP code_sexp, parsed;
  ParseStatus status;

  PROTECT(code_sexp = mkString(code));
  parsed = PROTECT(R_ParseVector(code_sexp, -1, &status, R_NilValue));

  if (status != PARSE_OK) {
    UNPROTECT(2);
    return R_NilValue;
  }

  UNPROTECT(2);
  return parsed;
}

static char* read_file(const char *path)
{
  FILE *f = fopen(path, "r");
  if (f == NULL) return NULL;

  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);

  char *content = malloc(size + 1);
  if (content == NULL) {
    fclose(f);
    return NULL;
  }

  size_t bytes = fread(content, 1, size, f);
  content[bytes] = '\0';
  fclose(f);
  return content;
}

int analyse_s7(RFile *files)
{
  LOG_INFO("Running S7 type checks...");

  S7Class *classes = NULL;
  S7Instance *instances = NULL;

  RFile *current = files;
  while (current != NULL) {
    if (current->dst == NULL) { current = current->next; continue; }

    char *content = read_file(current->dst);
    if (content == NULL) {
      LOG_WARNING("Failed to read %s for S7 check", current->dst);
      current = current->next;
      continue;
    }

    SEXP parsed = parse_code(content);
    if (parsed == R_NilValue) {
      LOG_WARNING("Failed to parse %s for S7 check", current->dst);
      free(content);
      current = current->next;
      continue;
    }

    PROTECT(parsed);
    R_xlen_t n = XLENGTH(parsed);
    for (R_xlen_t i = 0; i < n; i++) {
      walk_collect(VECTOR_ELT(parsed, i), &classes);
    }
    UNPROTECT(1);

    free(content);
    current = current->next;
  }

  current = files;
  while (current != NULL) {
    if (current->dst == NULL) { current = current->next; continue; }

    char *content = read_file(current->dst);
    if (content == NULL) { current = current->next; continue; }

    SEXP parsed = parse_code(content);
    if (parsed == R_NilValue) {
      free(content);
      current = current->next;
      continue;
    }

    PROTECT(parsed);
    R_xlen_t n = XLENGTH(parsed);
    for (R_xlen_t i = 0; i < n; i++) {
      walk_check(VECTOR_ELT(parsed, i), classes, &instances,
                 (int)i + 1, current->dst);
    }
    UNPROTECT(1);

    free(content);
    current = current->next;
  }

  free_instances(instances);
  free_classes(classes);
  return 0;
}
