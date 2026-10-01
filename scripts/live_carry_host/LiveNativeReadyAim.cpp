#include "LiveCarryStudy.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

bool FLiveCarryStudy::LoadNativeReadyAim()
{
    FString Text;TSharedPtr<FJsonObject> Json;
    if(!FFileHelper::LoadFileToString(Text,*(FPaths::ProjectSavedDir()/TEXT("LiveCarry/native-ready-aim.json")))||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Json)||!Json.IsValid())return false;
    const auto* Frames=&Json->GetArrayField(TEXT("frames"));if(Frames->Num()!=65)return false;
    for(const auto& Value:*Frames)
    {
        const auto F=Value->AsObject();if(!F.IsValid())return false;TArray<FTransform> CS,Local;
        for(const FName& N:Retarget.Names)
        {
            const TSharedPtr<FJsonObject>* T=nullptr;if(!F->TryGetObjectField(N.ToString(),T))return false;
            const auto& P=(*T)->GetArrayField(TEXT("p"));const auto& Q=(*T)->GetArrayField(TEXT("q"));const auto& S=(*T)->GetArrayField(TEXT("s"));
            if(P.Num()!=3||Q.Num()!=4||S.Num()!=3)return false;
            FTransform Pose(FQuat(Q[0]->AsNumber(),Q[1]->AsNumber(),Q[2]->AsNumber(),Q[3]->AsNumber()).GetNormalized(),FVector(P[0]->AsNumber(),P[1]->AsNumber(),P[2]->AsNumber()),FVector(S[0]->AsNumber(),S[1]->AsNumber(),S[2]->AsNumber()));
            if(Pose.ContainsNaN())return false;CS.Add(Pose);
        }
        if(CS.Num()!=131)return false;
        for(int I=0;I<CS.Num();++I)Local.Add(Retarget.Parents[I]>=0?CS[I].GetRelativeTransform(CS[Retarget.Parents[I]]):CS[I]);
        NativeReadyAim.Add(MoveTemp(CS));NativeReadyAimLocal.Add(MoveTemp(Local));
    }
    UE_LOG(LogTemp,Display,TEXT("LIVE_NATIVE_READY_AIM_READY bones=131 samples=65 original_helmet=1 moving_enabled=0 eye_finished=0"));return true;
}

bool FLiveCarryStudy::EvaluateNativeReadyAim(double Alpha,TArray<FTransform>& Local,TArray<FTransform>& CS)
{
    const double V=FMath::Clamp(Alpha,0.,1.)*64;const int A=FMath::FloorToInt(V),B=FMath::Min(A+1,64);const double W=V-A;
    Local.SetNum(131);CS.SetNum(131);
    // Native local FK preserves fixed-length upper/lower-arm links. Component
    // interpolation alone would shorten a bent limb between stored samples.
    for(int I=0;I<131;++I)
    {
        Local[I].Blend(NativeReadyAimLocal[A][I],NativeReadyAimLocal[B][I],W);
        CS[I]=Retarget.Parents[I]>=0?Local[I]*CS[Retarget.Parents[I]]:Local[I];
    }
    const int Gun=Retarget.Names.Find(TEXT("M4_body"));FTransform Mount;Mount.Blend(NativeReadyAim[A][Gun],NativeReadyAim[B][Gun],W);
    auto IsHeld=[&](int I)
    {
        if(Retarget.Names[I].ToString().StartsWith(TEXT("M4_")))return true;
        for(int J=I;J>=0;J=Retarget.Parents[J])if(Retarget.Names[J]==TEXT("DJ_wrist_L")||Retarget.Names[J]==TEXT("DJ_wrist_R"))return true;
        return false;
    };
    TArray<FTransform> Held=CS;
    for(int I=0;I<131;++I)if(IsHeld(I))Held[I]=Retarget.Hold[I].GetRelativeTransform(Retarget.Hold[Gun])*Mount;
    for(const TCHAR* Side:{TEXT("L"),TEXT("R")})
    {
        const int U=Retarget.Names.Find(FName(*FString::Printf(TEXT("UpperArm_%s"),Side))),E=Retarget.Names.Find(FName(*FString::Printf(TEXT("LowerArm_%s"),Side))),F=Retarget.Names.Find(FName(*FString::Printf(TEXT("DJ_forearm_%s"),Side))),H=Retarget.Names.Find(FName(*FString::Printf(TEXT("DJ_wrist_%s"),Side)));
        const FVector S=CS[U].GetTranslation(),Guide=CS[E].GetTranslation(),End=Held[H].GetTranslation();
        const double Upper=(Retarget.Bind[E].GetTranslation()-Retarget.Bind[U].GetTranslation()).Length(),Lower=(Retarget.Bind[H].GetTranslation()-Retarget.Bind[E].GetTranslation()).Length(),D=(End-S).Length();
        if(D>=Upper+Lower||D<=FMath::Abs(Upper-Lower))return false;
        const FVector Axis=(End-S)/D;const double Along=(Upper*Upper-Lower*Lower+D*D)/(2*D),Radius=FMath::Sqrt(Upper*Upper-Along*Along);
        const FVector Pole=(Guide-S-Axis*FVector::DotProduct(Guide-S,Axis)).GetSafeNormal();if(Pole.IsNearlyZero())return false;
        const FVector Elbow=S+Along*Axis+Radius*Pole;
        const FQuat Up=FQuat::FindBetweenNormals((Guide-S).GetSafeNormal(),(Elbow-S).GetSafeNormal());CS[U].SetRotation((Up*CS[U].GetRotation()).GetNormalized());
        const FQuat Low=FQuat::FindBetweenNormals((CS[H].GetTranslation()-Guide).GetSafeNormal(),(End-Elbow).GetSafeNormal());
        const FQuat Q=(Low*CS[E].GetRotation()).GetNormalized(),Deform=Q*Retarget.Bind[E].GetRotation().Inverse();
        CS[E].SetTranslation(Elbow);CS[E].SetRotation(Q);
        CS[F].SetTranslation(Elbow+Deform.RotateVector(Retarget.Bind[F].GetTranslation()-Retarget.Bind[E].GetTranslation()));CS[F].SetRotation((Deform*Retarget.Bind[F].GetRotation()).GetNormalized());
    }
    for(int I=0;I<131;++I)if(IsHeld(I))CS[I]=Held[I];
    for(int I=0;I<131;++I){if(CS[I].ContainsNaN())return false;Local[I]=Retarget.Parents[I]>=0?CS[I].GetRelativeTransform(CS[Retarget.Parents[I]]):CS[I];}
    return true;
}
