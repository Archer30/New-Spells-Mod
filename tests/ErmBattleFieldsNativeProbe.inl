// Test-only startup probe in a disposable game copy. Never packaged.
namespace BmgProbe
{
   int checks = 0, errors = 0, providerCalls = 0;
   const char* command = "setup";
   char lastCommand[512] = "setup";
   Patch* receiver = 0;
   CombatManager* fixture = 0;
   FILE* baseline = 0;

   void Check(bool condition, int line)
   {
      ++checks;
      if (!condition)
      {
         char text[512];
         sprintf_s(text, "FAIL line %d after %d checks: %s", line, checks, command);
         Era::WriteLog("BMG native probe", "Result", text);
         RaiseException(0xE0420213, 0, 0, 0);
      }
   }
#define BMG_CHECK(x) BmgProbe::Check(!!(x), __LINE__)

   void __stdcall Error(HiHook*, const char*, int, const char*) { ++errors; }
   void __stdcall ErrorMessage(HiHook*, void* subcommand)
   {
      ++errors;
      // ERA TErmSubCmd: Pos, Code.Value, Code.Len. Skip this command exactly
      // as Hook_ErmMess does, without showing a modal in an automated probe.
      int& pos = *static_cast<int*>(subcommand);
      const char* code = *reinterpret_cast<const char**>(
         static_cast<unsigned char*>(subcommand) + 4);
      while (code[pos] && code[pos] != ';') ++pos;
   }

   army* Stack(int slot)
   {
      return reinterpret_cast<army*>(&fixture->stack[slot / 21][slot % 21]);
   }

   void Seed(bool patched, bool poisonOldMastery = true)
   {
      std::memset(fixture, 0, sizeof(*fixture));
      std::memset(activeSpellMastery, 0, sizeof(activeSpellMastery));
      for (int slot = 0; slot < 42; ++slot)
      {
         army* stack = Stack(slot);
         stack->group = slot / 21; stack->index = slot % 21;
         stack->armyType = static_cast<TCreatureType>(0);
         stack->numTroops = stack->origNumTroops = 10;
         stack->sMonInfo.hitPoints = stack->origHitPoints = 20;
         stack->sMonInfo.speed = 10; stack->gridIndex = 93;
         stack->iMorale = -2; stack->iLuck = 3;
         stack->numTroopsBattleResurrected = 4;
         for (int spell = 0; spell <= 80 && poisonOldMastery; ++spell)
         {
            const int value = slot * 1000 + spell + 10;
            activeSpellMastery[slot / 21][slot % 21][spell] = value;
            // Distinct old physical cells prove that the new handler uses
            // relocated mastery, including aliases into a different stack.
            *reinterpret_cast<int*>(reinterpret_cast<unsigned char*>(stack) +
               0x2DC + 4 * spell) = patched ? -123456 : value;
         }
      }
      if (receiver) {
         if (patched) { receiver->Apply(); BMG_CHECK(receiver->IsApplied()); }
         else { receiver->Undo(); BMG_CHECK(!receiver->IsApplied()); }
      }
   }

   struct Result { int first, second, third, fourth, errorCount; bool flag; };
   Result Run(const char* text)
   {
      strcpy_s(lastCommand, text);
      command = lastCommand;
      errors = 0;
      for (int i = 9901; i <= 9904; ++i) Era::v[i] = -99999;
      Era::f[1] = false;
      // ExecErmCmd accepts receiver text without the script-only !! prefix.
      const char* begin = text;
      while (*begin) {
         if (begin[0] == '!' && begin[1] == '!') begin += 2;
         const char* end = std::strchr(begin, ';');
         char single[512];
         const size_t length = end ? end - begin + 1 : std::strlen(begin);
         memcpy(single, begin, length); single[length] = 0;
         Era::ExecErmCmd(single);
         begin += length;
      }
      Result result = {Era::v[9901], Era::v[9902], Era::v[9903], Era::v[9904], errors, Era::f[1]};
      return result;
   }

   int Exception(EXCEPTION_POINTERS* info)
   {
      MEMORY_BASIC_INFORMATION region = {};
      VirtualQuery(info->ExceptionRecord->ExceptionAddress, &region, sizeof(region));
      char module[MAX_PATH] = {};
      GetModuleFileNameA(static_cast<HMODULE>(region.AllocationBase), module, MAX_PATH);
      char text[1024];
      sprintf_s(text, "Exception %08X at %p, module %s +%08X; command %s",
         info->ExceptionRecord->ExceptionCode, info->ExceptionRecord->ExceptionAddress,
         module, static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(info->ExceptionRecord->ExceptionAddress) -
            reinterpret_cast<std::uintptr_t>(region.AllocationBase)), lastCommand);
      Era::WriteLog("BMG native probe", "Exception", text);
      sprintf_s(text, "EAX %08X EBX %08X ECX %08X EDX %08X ESP %08X EBP %08X; command address %p",
         info->ContextRecord->Eax, info->ContextRecord->Ebx, info->ContextRecord->Ecx,
         info->ContextRecord->Edx, info->ContextRecord->Esp, info->ContextRecord->Ebp, command);
      Era::WriteLog("BMG native probe", "Registers", text);
      for (int i = 0; i < 16; ++i) {
         sprintf_s(text, "ESP+%02X: %08X", i * 4, reinterpret_cast<unsigned*>(info->ContextRecord->Esp)[i]);
         Era::WriteLog("BMG native probe", "Stack", text);
      }
      return EXCEPTION_EXECUTE_HANDLER;
   }

   void Compare(const char* text, bool expectError = false)
   {
      Seed(false);
      const Result original = Run(text);
      BMG_CHECK((original.errorCount != 0) == expectError);
#ifdef NEWSPELLS_BMG_BASELINE_PROBE
      BMG_CHECK(fwrite(&original, sizeof(original), 1, baseline) == 1);
#else
      Result standalone = {};
      BMG_CHECK(fread(&standalone, sizeof(standalone), 1, baseline) == 1);
      BMG_CHECK(original.first == standalone.first && original.second == standalone.second &&
         original.third == standalone.third && original.fourth == standalone.fourth &&
         original.flag == standalone.flag && original.errorCount == standalone.errorCount);
      Seed(true);
      const Result patched = Run(text);
      BMG_CHECK(original.first == patched.first && original.second == patched.second &&
         original.third == patched.third && original.fourth == patched.fourth &&
         original.flag == patched.flag && original.errorCount == patched.errorCount);
#endif
   }

   int32_t __stdcall Provider(NewSpellsCombatContextV1* context)
   {
      ++providerCalls;
      BMG_CHECK(context->spellId == 96 && context->source == NEWSPELLS_SOURCE_ERM);
      return NEWSPELLS_PROVIDER_COMMITTED;
   }

   void Body()
   {
      receiver = _P->GetLastPatchAt(0x75F334);
#ifdef NEWSPELLS_BMG_BASELINE_PROBE
      BMG_CHECK(!receiver);
      BMG_CHECK(fopen_s(&baseline, "Debug\\bmg-original-results.bin", "wb") == 0);
#else
      BMG_CHECK(receiver && receiver->GetType() == LOHOOK_ && receiver->IsApplied());
      BMG_CHECK(fopen_s(&baseline, "Debug\\bmg-original-results.bin", "rb") == 0);
#endif
      char text[320];
      const int stacks[] = {0, 20, 21, 41};
      for (int s = 0; s < 4; ++s)
         for (int index = -102; index <= 235; ++index)
         {
            if (index >= 81 && index <= 126) continue;
            sprintf_s(text, "!!BM%d:G%d/?v9901/?v9902;", stacks[s], index);
            Compare(text);
         }
      const int fields[] = {-103, -100, -95, -93, -92, -91, -87, -86, -81,
         -75, -74, -73, -70, -68, -67, -66, -13, -12, 127, 128, 154, 155,
         161, 176, 195, 200, 206, 207, 209, 212, 213, 235, 236, 465};
      for (int i = 0; i < sizeof(fields) / sizeof(fields[0]); ++i)
      {
         const int n = fields[i];
         sprintf_s(text, "!!BM1:G%d/-3/d G%d/d1/d G%d/?v9901/?v9902 G%d/>=-2/d;", n, n, n, n);
         Compare(text);
      }
      Compare("!!BM0:G195/1056964608/d G195/?v9901/d;"); // 0.5f bits
      Compare("!!BM0:G-93/d/12345 G-93/?v9901/?v9902;"); // second field write
      Compare("!!BM0:G-81/d/7 G0/?v9901/d;"); // second field is native duration[0]
      Compare("!!BM0:G213/-3/12345 G213/?v9901/?v9902;"); // actual cross-stack secondary
      Compare("!!BM0:G2147483647/d/d;");
      Compare("!!BM0:G-2147483648/d/d;");
      // Original native operands were never restricted to duration>=0/mastery<=3.
      Compare("!!BM0:G27/-5/7 G27/?v9901/?v9902;");
      Compare("!!BM0:G60/-1/-2 G60/?v9901/?v9902;");
      // Physical identity remains valid even if a script edits side/index.
      Compare("!!BM0:I1 G212/?v9901/d G127/?v9902/d;");
      Compare("!!BM0:I1 G27/-5/7 G27/?v9901/?v9902;");
      Compare("!!BM0:G212/1;", true);
      Compare("!!BM42:G212/?v9901/d;", true);
      Compare("!!BM0:G?v9901/d/d;", true);
      Compare("!!BM0:G212/9/?v10001;", true);
      Compare("!!BM0:G27/-5/?v10001;", true);
      Compare("!!BM0:G27/?v10001/-5;", true);
      Compare("!!BM0:G27/1;", true);
      // A failed second operand does not undo the first assignment.
      Compare("!!BM0:G213/-9/?v10001;!!BM0:G213/?v9901/d;", true);
      Compare("!!BM0:G27/-9/?v10001;!!BM0:G27/?v9901/d;", true);
      Compare("!!BM1:I0 G465/-8/d G465/?v9901/d;!!BM2:G46/d/?v9902;");
      Compare("!!BM0:G384/d/-8;!!BM1:G46/d/?v9901;");

      // ACM attack reads/clears, Night Scouting casualties, Grand Manouvre restore.
      Compare("!!BM0:G213/?v9901/?t I?v9903;!!BM21:G213/?v9902/?t I?v9904;!!BM0:G213/0/?t;!!BM21:G213/0/?t;");
      Compare("!!BM21:N?v9901 T?v9902 G-81/?v9903/d;!!VRv9901:-v9903;");
      Compare("!!BM0:G212/?v9901/d G212/0/d G212/v9901/d G212/?v9902/d;");
      fclose(baseline); baseline = 0;
#ifdef NEWSPELLS_BMG_BASELINE_PROBE
      return;
#endif

      Seed(true, false);
      BMG_CHECK(Run("!!BM0:G81/3/2 G81/?v9901/?v9902;").errorCount == 0);
      BMG_CHECK(Era::v[9901] == 3 && Era::v[9902] == 2 && Stack(0)->spellInfluence[81] == 3);
      BMG_CHECK(Run("!!BM0:G81/-1/4;").errorCount > 0);
      BMG_CHECK(Stack(0)->spellInfluence[81] == 3 && activeSpellMastery[0][0][81] == 2);
      BMG_CHECK(Run("!!BM0:G126/4/2;").errorCount > 0);

      ExternalSpellSlot& external = *getExternalSpellSlot(96);
      const ExternalSpellSlot saved = external;
      external = ExternalSpellSlot();
      external.active = external.registered = true;
      external.descriptor.capabilities = NEWSPELLS_CAP_ERM_CAST;
      external.descriptor.OnErmCast = Provider;
      BMG_CHECK(Run("!!BM0:G96/3/2 G96/?v9901/?v9902;").errorCount == 0);
      BMG_CHECK(providerCalls == 1 && Era::v[9901] == 3 && Era::v[9902] == 2);
      external.active = false;
      BMG_CHECK(Run("!!BM0:G96/4/2;").errorCount > 0);
      BMG_CHECK(Stack(0)->spellInfluence[96] == 3 && providerCalls == 1);
      external.active = true; external.descriptor.capabilities = 0;
      BMG_CHECK(Run("!!BM0:G96/4/2;").errorCount > 0);
      external = saved;

      // Real native apply/removal, including an added status spell.
      Seed(true, false);
      Stack(0)->SetSpellInfluence(SPELL_HASTE, 4, 3, 0);
      BMG_CHECK(Stack(0)->spellInfluence[SPELL_HASTE] == 4 && Stack(0)->SpellInfluenceQueue.size == 1);
      BMG_CHECK(Run("!!BM0:G53/0/d;").errorCount == 0);
      BMG_CHECK(Stack(0)->spellInfluence[SPELL_HASTE] == 0 &&
         Stack(0)->SpellInfluenceQueue.size == 0 && activeSpellMastery[0][0][SPELL_HASTE] == 0);
      Stack(0)->SetSpellInfluence(SPELL_FEAR, 4, 2, 0);
      BMG_CHECK(Stack(0)->spellInfluence[SPELL_FEAR] == 4);
      BMG_CHECK(Run("!!BM0:G81/0/d;").errorCount == 0);
      BMG_CHECK(Stack(0)->spellInfluence[SPELL_FEAR] == 0 && Stack(0)->SpellInfluenceQueue.size == 0);
   }
}

void RunBmgNativeProbe()
{
   CombatManager* saved = pCombatManager;
   BmgProbe::fixture = reinterpret_cast<CombatManager*>(o_New(sizeof(CombatManager)));
   pCombatManager = BmgProbe::fixture;
   PatcherInstance* owner = _P->CreateInstance("NewSpells.Test.BMG");
   owner->WriteHiHook(0x712333, SPLICE_, EXTENDED_, CDECL_, BmgProbe::Error);
   owner->WriteHiHook(0x73DE8A, SPLICE_, EXTENDED_, CDECL_, BmgProbe::ErrorMessage);
   bool passed = false;
   __try { BmgProbe::Body(); passed = true; }
   __except (BmgProbe::Exception(GetExceptionInformation())) {}
   pCombatManager = saved;
   if (BmgProbe::receiver) BmgProbe::receiver->Apply();
   owner->UndoAll();
   if (BmgProbe::baseline) fclose(BmgProbe::baseline);
   char text[80]; sprintf_s(text, "%s: %d checks", passed ? "PASS" : "FAIL", BmgProbe::checks);
   Era::WriteLog("BMG native probe", "Result", text);
   // This binary is only run in a disposable sandbox; never enter gameplay
   // with a synthetic stack fixture or any temporary native allocation.
   ExitProcess(passed ? 0 : 1);
}
#undef BMG_CHECK
