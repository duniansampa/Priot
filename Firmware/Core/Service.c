#include "Service.h"
#include "Tools.h"
#include "ReadConfig.h"
#include "Debug.h"

static char** _Service_createWordArrayHelper(const char* cptr, size_t idx, char* tmp, size_t tmplen)
{
    char* item;
    char** res;
    cptr = ReadConfig_copyNwordConst(cptr, tmp, tmplen);
    item = strdup(tmp);
    if (cptr)
        res = _Service_createWordArrayHelper(cptr, idx + 1, tmp, tmplen);
    else {
        res = (char**)malloc(sizeof(char*) * (idx + 2));
        res[idx + 1] = NULL;
    }
    res[idx] = item;
    return res;
}

static char** _Service_createWordArray(const char* cptr)
{
    size_t tmplen = strlen(cptr);
    char* tmp = (char*)malloc(tmplen + 1);
    char** res = _Service_createWordArrayHelper(cptr, 0, tmp, tmplen + 1);
    free(tmp);
    return res;
}

static void _Service_destroyWordArray(char** arr)
{
    if (arr) {
        char** run = arr;
        while(*run) {
            free(*run);
            ++run;
        }
        free(arr);
    }
}

struct Service_LookupDomain_s {
    char* application;
    char** userDomain;
    char** domain;
    struct Service_LookupDomain_s* next;
};

static struct Service_LookupDomain_s* _service_domains = NULL;

int Service_registerDefaultDomain(const char* application, const char* domain)
{
    struct Service_LookupDomain_s *run = _service_domains, *prev = NULL;
    int res = 0;

    while (run != NULL && strcmp(run->application, application) < 0) {
    prev = run;
    run = run->next;
    }
    if (run && strcmp(run->application, application) == 0) {
      if (run->domain != NULL) {
          _Service_destroyWordArray(run->domain);
      run->domain = NULL;
      res = 1;
      }
    } else {
    run = TOOLS_MALLOC_STRUCT(Service_LookupDomain_s);
    run->application = strdup(application);
    run->userDomain = NULL;
    if (prev) {
        run->next = prev->next;
        prev->next = run;
    } else {
        run->next = _service_domains;
        _service_domains = run;
    }
    }
    if (domain) {
        run->domain = _Service_createWordArray(domain);
    } else if (run->userDomain == NULL) {
    if (prev)
        prev->next = run->next;
    else
        _service_domains = run->next;
    free(run->application);
    free(run);
    }
    return res;
}

void Service_clearDefaultDomain(void)
{
    while (_service_domains) {
    struct Service_LookupDomain_s *tmp = _service_domains;
    _service_domains = _service_domains->next;
    free(tmp->application);
        _Service_destroyWordArray(tmp->userDomain);
        _Service_destroyWordArray(tmp->domain);
    free(tmp);
    }
}

static void _Service_registerUserDomain(const char* token, char* cptr)
{
    struct Service_LookupDomain_s *run = _service_domains, *prev = NULL;
    size_t len = strlen(cptr) + 1;
    char* application = (char*)malloc(len);
    char** domain;

    {
        char* cp = ReadConfig_copyNword(cptr, application, len);
        if (cp == NULL) {
            ReadConfig_error("No domain(s) in registration of "
                                 "defDomain \"%s\"", application);
            free(application);
            return;
        }
        domain = _Service_createWordArray(cp);
    }

    while (run != NULL && strcmp(run->application, application) < 0) {
    prev = run;
    run = run->next;
    }
    if (run && strcmp(run->application, application) == 0) {
    if (run->userDomain != NULL) {
        ReadConfig_configPerror("Default transport already registered for this "
              "application");
            _Service_destroyWordArray(domain);
            free(application);
        return;
    }
    } else {
    run = TOOLS_MALLOC_STRUCT(Service_LookupDomain_s);
    run->application = strdup(application);
    run->domain = NULL;
    if (prev) {
        run->next = prev->next;
        prev->next = run;
    } else {
        run->next = _service_domains;
        _service_domains = run;
    }
    }
    run->userDomain = domain;
    free(application);
}

static void _Service_clearUserDomain(void)
{
    struct Service_LookupDomain_s *run = _service_domains, *prev = NULL;

    while (run) {
    if (run->userDomain != NULL) {
            _Service_destroyWordArray(run->userDomain);
        run->userDomain = NULL;
    }
    if (run->domain == NULL) {
        struct Service_LookupDomain_s *tmp = run;
        if (prev)
        run = prev->next = run->next;
        else
        run = _service_domains = run->next;
        free(tmp->application);
        free(tmp);
    } else {
        prev = run;
        run = run->next;
    }
    }
}

const char* const * Service_lookupDefaultDomains(const char* application)
{
    const char * const * res;

    if (application == NULL)
    res = NULL;
    else {
        struct Service_LookupDomain_s *run = _service_domains;

    while (run && strcmp(run->application, application) < 0)
        run = run->next;
    if (run && strcmp(run->application, application) == 0)
        if (run->userDomain)
                res = (const char * const *)run->userDomain;
        else
                res = (const char * const *)run->domain;
    else
        res = NULL;
    }
    DEBUG_MSGTL(("defaults",
                "Service_lookupDefaultDomains(\"%s\") ->",
                application ? application : "[NIL]"));
    if (res) {
        const char * const * r = res;
        while(*r) {
            DEBUG_MSG(("defaults", " \"%s\"", *r));
            ++r;
        }
        DEBUG_MSG(("defaults", "\n"));
    } else
        DEBUG_MSG(("defaults", " \"[NIL]\"\n"));
    return res;
}

const char* Service_lookupDefaultDomain(const char* application)
{
    const char * const * res = Service_lookupDefaultDomains(application);
    return (res ? *res : NULL);
}

struct Service_LookupTarget_s {
    char* application;
    char* domain;
    char* userTarget;
    char* target;
    struct Service_LookupTarget_s* next;
};

static struct Service_LookupTarget_s* _service_targets = NULL;

/**
 * Add an (application, domain, target) triplet to the targets list if target
 * != NULL. Remove an entry if target == NULL and the userTarget pointer for
 * the entry found is also NULL. Keep at most one target per (application,
 * domain) pair.
 *
 * @return 1 if an entry for (application, domain) was already present in the
 *   targets list or 0 if such an entry was not yet present in the targets list.
 */
int Service_registerDefaultTarget(const char* application, const char* domain,
                const char* target)
{
    struct Service_LookupTarget_s *run = _service_targets, *prev = NULL;
    int i = 0, res = 0;
    while (run && ((i = strcmp(run->application, application)) < 0 ||
           (i == 0 && strcmp(run->domain, domain) < 0))) {
    prev = run;
    run = run->next;
    }
    if (run && i == 0 && strcmp(run->domain, domain) == 0) {
      if (run->target != NULL) {
        free(run->target);
        run->target = NULL;
        res = 1;
      }
    } else {
    run = TOOLS_MALLOC_STRUCT(Service_LookupTarget_s);
    run->application = strdup(application);
    run->domain = strdup(domain);
    run->userTarget = NULL;
    if (prev) {
        run->next = prev->next;
        prev->next = run;
    } else {
        run->next = _service_targets;
        _service_targets = run;
    }
    }
    if (target) {
    run->target = strdup(target);
    } else if (run->userTarget == NULL) {
    if (prev)
        prev->next = run->next;
    else
        _service_targets = run->next;
    free(run->domain);
    free(run->application);
    free(run);
    }
    return res;
}

/**
 * Clear the targets list.
 */
void Service_clearDefaultTarget(void)
{
    while (_service_targets) {
    struct Service_LookupTarget_s *tmp = _service_targets;
    _service_targets = _service_targets->next;
    free(tmp->application);
    free(tmp->domain);
    free(tmp->userTarget);
    free(tmp->target);
    free(tmp);
    }
}

static void _Service_registerUserTarget(const char* token, char* cptr)
{
    struct Service_LookupTarget_s *run = _service_targets, *prev = NULL;
    size_t len = strlen(cptr) + 1;
    char* application = (char*)malloc(len);
    char* domain = (char*)malloc(len);
    char* target = (char*)malloc(len);
    int i = 0;

    {
    char* cp = ReadConfig_copyNword(cptr, application, len);
        if (cp == NULL) {
            ReadConfig_error("No domain and target in registration of "
                                 "defTarget \"%s\"", application);
            goto goto_done;
        }
    cp = ReadConfig_copyNword(cp, domain, len);
        if (cp == NULL) {
            ReadConfig_error("No target in registration of "
                                 "defTarget \"%s\" \"%s\"",
                                 application, domain);
            goto goto_done;
        }
    cp = ReadConfig_copyNword(cp, target, len);
    if (cp)
        ReadConfig_configPerror("Trailing junk found");
    }

    while (run && ((i = strcmp(run->application, application)) < 0 ||
           (i == 0 && strcmp(run->domain, domain) < 0))) {
    prev = run;
    run = run->next;
    }
    if (run && i == 0 && strcmp(run->domain, domain) == 0) {
    if (run->userTarget != NULL) {
        ReadConfig_configPerror("Default target already registered for this "
              "application-domain combination");
        goto goto_done;
    }
    } else {
    run = TOOLS_MALLOC_STRUCT(Service_LookupTarget_s);
    run->application = strdup(application);
    run->domain = strdup(domain);
    run->target = NULL;
    if (prev) {
        run->next = prev->next;
        prev->next = run;
    } else {
        run->next = _service_targets;
        _service_targets = run;
    }
    }
    run->userTarget = strdup(target);
 goto_done:
    free(target);
    free(domain);
    free(application);
}

static void _Service_clearUserTarget(void)
{
    struct Service_LookupTarget_s *run = _service_targets, *prev = NULL;

    while (run) {
    if (run->userTarget != NULL) {
        free(run->userTarget);
        run->userTarget = NULL;
    }
    if (run->target == NULL) {
        struct Service_LookupTarget_s *tmp = run;
        if (prev)
        run = prev->next = run->next;
        else
        run = _service_targets = run->next;
        free(tmp->application);
        free(tmp->domain);
        free(tmp);
    } else {
        prev = run;
        run = run->next;
    }
    }
}

const char* Service_lookupDefaultTarget(const char* application, const char* domain)
{
    int i = 0;
    struct Service_LookupTarget_s *run = _service_targets;
    const char *res;

    if (application == NULL || domain == NULL)
    res = NULL;
    else {
    while (run && ((i = strcmp(run->application, application)) < 0 ||
               (i == 0 && strcmp(run->domain, domain) < 0)))
        run = run->next;
    if (run && i == 0 && strcmp(run->domain, domain) == 0)
        if (run->userTarget != NULL)
        res = run->userTarget;
        else
        res = run->target;
    else
        res = NULL;
    }
    DEBUG_MSGTL(("defaults",
        "Service_lookupDefaultTarget(\"%s\", \"%s\") -> \"%s\"\n",
        application ? application : "[NIL]",
        domain ? domain : "[NIL]",
        res ? res : "[NIL]"));
    return res;
}

void Service_registerServiceHandlers(void)
{
    ReadConfig_registerConfigHandler("priot:", "defDomain",
                _Service_registerUserDomain,
                _Service_clearUserDomain,
                "application domain");
    ReadConfig_registerConfigHandler("priot:", "defTarget",
                _Service_registerUserTarget,
                _Service_clearUserTarget,
                "application domain target");
}
