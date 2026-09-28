#include "LiveCarryRetarget.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"

namespace {
FVector Vec(const TArray<TSharedPtr<FJsonValue>>& A){return FVector(A[0]->AsNumber(),A[1]->AsNumber(),A[2]->AsNumber());}
FTransform Read(const TSharedPtr<FJsonObject>& O){const auto& Q=O->GetArrayField(TEXT("q"));return FTransform(FQuat(Q[0]->AsNumber(),Q[1]->AsNumber(),Q[2]->AsNumber(),Q[3]->AsNumber()).GetNormalized(),Vec(O->GetArrayField(TEXT("p"))),Vec(O->GetArrayField(TEXT("s"))));}
FQuat Between(const FVector& A,const FVector& B){return FQuat::FindBetweenNormals(A.GetSafeNormal(),B.GetSafeNormal()).GetNormalized();}
FQuat Frame(const FVector& Long,const FVector& Across){const FVector X=Long.GetSafeNormal(),Y=(Across-X*FVector::DotProduct(Across,X)).GetSafeNormal(),Z=FVector::CrossProduct(X,Y);FMatrix M=FMatrix::Identity;M.SetAxes(&X,&Y,&Z);return FQuat(M).GetNormalized();}
FQuat RotationVector(const FVector& V){const double A=V.Size();return A>1.e-8?FQuat(V/A,A):FQuat::Identity;}
}
bool FLiveCarryRetarget::Load(const FString& File)
{
    FString Text;TSharedPtr<FJsonObject> Root;
    if(!FFileHelper::LoadFileToString(Text,*File)||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Root))return false;
    auto Target=Root->GetObjectField(TEXT("target"));const auto Ref=Target->GetObjectField(TEXT("reference")),H=Target->GetObjectField(TEXT("hold")),P=Target->GetObjectField(TEXT("parents"));
    for(const auto& N:Target->GetArrayField(TEXT("names"))){FName Name(*N->AsString());TargetIndex.Add(Name,Names.Num());Names.Add(Name);Bind.Add(Read(Ref->GetObjectField(Name.ToString())));Hold.Add(Read(H->GetObjectField(Name.ToString())));RawZero.Add(Read(Root->GetObjectField(TEXT("raw_zero"))->GetObjectField(Name.ToString())));}
    for(int I=0;I<Names.Num();++I){FString Parent;const int J=P->TryGetStringField(Names[I].ToString(),Parent)?TargetIndex.FindRef(FName(*Parent)):-1;Parents.Add(J);LocalBind.Add(J>=0?Bind[I].GetRelativeTransform(Bind[J]):Bind[I]);HoldLocal.Add(J>=0?Hold[I].GetRelativeTransform(Hold[J]):Hold[I]);}
    const auto SR=Root->GetObjectField(TEXT("source_reference"));
    for(const auto& Item:SR->Values){FName N(*Item.Key);SourceIndex.Add(N,SourceNames.Num());SourceNames.Add(N);SourceBind.Add(Read(Item.Value->AsObject()));}
    LegScale=Root->GetNumberField(TEXT("leg_scale"));Amplitude=Root->GetNumberField(TEXT("amplitude"));
    LiveRightPoleDelta=Root->GetNumberField(TEXT("live_right_pole_delta"));
    const auto Jog=Root->GetObjectField(TEXT("jog_registration"));JogTranslation=Vec(Jog->GetArrayField(TEXT("chest_translation_cm")));for(int I=0;I<2;++I)JogPole[I]=Jog->GetArrayField(TEXT("elbow_plane_radians"))[I]->AsNumber();
    for(int I=0;I<2;++I)for(const auto& V:Root->GetObjectField(TEXT("sole_points"))->GetArrayField(I==0?TEXT("L"):TEXT("R")))
    {
        TArray<FSoleInfluence> Point;
        for(const auto& Value:V->AsArray()){auto O=Value->AsObject();Point.Add({TargetIndex.FindChecked(FName(*O->GetStringField(TEXT("bone")))),O->GetNumberField(TEXT("weight")),Vec(O->GetArrayField(TEXT("local")))});}
        SolePoints[I].Add(MoveTemp(Point));
    }
    const auto& Fit=Root->GetArrayField(TEXT("fit"));FitRotation=RotationVector(FVector(Fit[0]->AsNumber(),Fit[1]->AsNumber(),Fit[2]->AsNumber()));FitTranslation=FVector(Fit[3]->AsNumber(),Fit[4]->AsNumber(),Fit[5]->AsNumber());PoleAngles[0]=Fit[6]->AsNumber();PoleAngles[1]=Fit[7]->AsNumber();
    const int L=TargetIndex.FindChecked(TEXT("DJ_wrist_L")),R=TargetIndex.FindChecked(TEXT("DJ_wrist_R")),Pelvis=TargetIndex.FindChecked(TEXT("Pelvis"));
    HoldMid=(Hold[L].GetTranslation()+Hold[R].GetTranslation())*.5;HoldSpan=Hold[L].GetTranslation()-Hold[R].GetTranslation();
    const FTransform S0=Read(Root->GetObjectField(TEXT("source_zero"))->GetObjectField(TEXT("pelvis")));
    FloorShift=RawZero[Pelvis].GetTranslation().Z-Bind[Pelvis].GetTranslation().Z-(S0.GetTranslation().Z-SourceBind[SourceIndex.FindChecked(TEXT("pelvis"))].GetTranslation().Z)*LegScale;
    FQuat Chest;Assembly(RawZero,AssemblyZero,CenterZero,Chest);
    return Names.Num()==131;
}
bool FLiveCarryRetarget::IsBelow(int32 Bone,int32 Parent) const{for(int I=Bone;I>=0;I=Parents[I])if(I==Parent)return true;return false;}
bool FLiveCarryRetarget::LoadAim(const FString& File)
{
    FString Text;TSharedPtr<FJsonObject> Root;
    if(!FFileHelper::LoadFileToString(Text,*File)||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Root))return false;
    const auto& Fit=Root->GetArrayField(TEXT("fit"));if(Fit.Num()!=8)return false;
    AimRotation=RotationVector(FVector(Fit[0]->AsNumber(),Fit[1]->AsNumber(),Fit[2]->AsNumber()));ReadyTranslation=FVector(Fit[3]->AsNumber(),Fit[4]->AsNumber(),Fit[5]->AsNumber());ReadyPole[0]=Fit[6]->AsNumber();ReadyPole[1]=Fit[7]->AsNumber();
    const auto& Aim=Root->GetArrayField(TEXT("aim_registration"));if(Aim.Num()!=8)return false;
    AimTranslation=FVector(Aim[3]->AsNumber(),Aim[4]->AsNumber(),Aim[5]->AsNumber());AimPole[0]=Aim[6]->AsNumber();AimPole[1]=Aim[7]->AsNumber();
    if(Root->HasField(TEXT("moving_aim_pole_delta"))){const auto& Delta=Root->GetArrayField(TEXT("moving_aim_pole_delta"));if(Delta.Num()!=2)return false;for(int Side=0;Side<2;++Side)MovingAimPoleDelta[Side]=Delta[Side]->AsNumber();}
    return true;
}
bool FLiveCarryRetarget::TwoBone(const FVector& S,const FVector& E,const FVector& T,double A,double B,double Phi,FVector& Result) const
{
    const double D=(T-S).Size();if(D<.001||D>=A+B||D<=FMath::Abs(A-B))return false;
    const FVector Axis=(T-S)/D;const double Along=(A*A-B*B+D*D)/(2*D);const double Radius=FMath::Sqrt(FMath::Max(1.e-8,A*A-Along*Along));
    const FVector Pole=(E-S-Axis*FVector::DotProduct(E-S,Axis)).GetSafeNormal();if(Pole.IsNearlyZero())return false;
    Result=S+Axis*Along+FQuat(Axis,Phi).RotateVector(Pole)*Radius;return true;
}
void FLiveCarryRetarget::Assembly(const TArray<FTransform>& P,FQuat& Q,FVector& C,FQuat& Chest) const
{
    const int L=TargetIndex.FindChecked(TEXT("DJ_wrist_L")),R=TargetIndex.FindChecked(TEXT("DJ_wrist_R")),Ch=TargetIndex.FindChecked(TEXT("Chest"));
    Q=P[R].GetRotation()*Hold[R].GetRotation().Inverse();Q=Between(Q.RotateVector(HoldSpan),P[L].GetTranslation()-P[R].GetTranslation())*Q;
    Chest=P[Ch].GetRotation()*Bind[Ch].GetRotation().Inverse();C=Chest.UnrotateVector((P[L].GetTranslation()+P[R].GetTranslation())*.5-P[Ch].GetTranslation());Q=Chest.Inverse()*Q;
}
bool FLiveCarryRetarget::Evaluate(const TArray<FTransform>& Input,const TArray<FName>& Order,TArray<FTransform>& Local,TArray<FTransform>& P,bool LiveClearance)
{
    TArray<FTransform> S;S.SetNum(SourceNames.Num());for(int I=0;I<SourceNames.Num();++I){int J=Order.Find(SourceNames[I]);if(J<0||!Input.IsValidIndex(J))return false;S[I]=Input[J];}
    auto SB=[&](const FString& N)->const FTransform&{return SourceBind[SourceIndex.FindChecked(FName(*N))];};
    auto SP=[&](const FString& N)->const FTransform&{return S[SourceIndex.FindChecked(FName(*N))];};
    auto Deform=[&](const FString& N){return SP(N).GetRotation()*SB(N).GetRotation().Inverse();};
    auto Palm=[&](const FString& Side){return Frame(SP(TEXT("middle_01_")+Side).GetTranslation()-SP(TEXT("hand_")+Side).GetTranslation(),SP(TEXT("index_01_")+Side).GetTranslation()-SP(TEXT("pinky_01_")+Side).GetTranslation());};
    auto Index=[&](const FString& N){return TargetIndex.FindChecked(FName(*N));};
    const TMap<FName,FString> Map={{TEXT("Pelvis"),TEXT("pelvis")},{TEXT("Torso"),TEXT("spine_01")},{TEXT("Chest"),TEXT("spine_03")},{TEXT("Neck"),TEXT("neck_01")},{TEXT("Head"),TEXT("head")}};
    P.SetNum(Names.Num());
    for(int I=0;I<Names.Num();++I)
    {
        P[I]=Parents[I]>=0?LocalBind[I]*P[Parents[I]]:Bind[I];
        if(const auto* N=Map.Find(Names[I])){P[I].SetRotation((Deform(*N)*Bind[I].GetRotation()).GetNormalized());if(Names[I]==TEXT("Pelvis"))P[I].SetTranslation(Bind[I].GetTranslation()+(SP(TEXT("pelvis")).GetTranslation()-SB(TEXT("pelvis")).GetTranslation())*LegScale);}
        for(const FString Side:{FString(TEXT("L")),FString(TEXT("R"))})
        {
            const FString Low=Side.ToLower();const int Sh=Index(TEXT("Shoulder_")+Side),U=Index(TEXT("UpperArm_")+Side),E=Index(TEXT("LowerArm_")+Side),W=Index(TEXT("DJ_wrist_")+Side);
            const FVector BU=Bind[U].GetTranslation(),BE=Bind[E].GetTranslation(),BW=Bind[W].GetTranslation();
            if(I==Sh)P[I].SetRotation((Deform(TEXT("clavicle_")+Low)*Between(BU-Bind[Sh].GetTranslation(),SB(TEXT("upperarm_")+Low).GetTranslation()-SB(TEXT("clavicle_")+Low).GetTranslation())*Bind[I].GetRotation()).GetNormalized());
            if(I==U)P[I].SetRotation((Deform(TEXT("upperarm_")+Low)*Between(BE-BU,SB(TEXT("lowerarm_")+Low).GetTranslation()-SB(TEXT("upperarm_")+Low).GetTranslation())*Bind[I].GetRotation()).GetNormalized());
            if(I==E)P[I].SetRotation((Deform(TEXT("lowerarm_")+Low)*Between(BW-BE,SB(TEXT("hand_")+Low).GetTranslation()-SB(TEXT("lowerarm_")+Low).GetTranslation())*Bind[I].GetRotation()).GetNormalized());
            if(I==Index(TEXT("DJ_forearm_")+Side))P[I]=Bind[I].GetRelativeTransform(Bind[E])*P[E];
            if(I==W){const FQuat HP=Frame(Hold[Index(TEXT("DJ_middle_01_")+Side)].GetTranslation()-Hold[W].GetTranslation(),Hold[Index(TEXT("DJ_index_01_")+Side)].GetTranslation()-Hold[Index(TEXT("DJ_pinky_01_")+Side)].GetTranslation());P[I]=FTransform((Palm(Low)*HP.Inverse()*Hold[W].GetRotation()).GetNormalized(),P[E].GetTranslation()+(SP(TEXT("hand_")+Low).GetTranslation()-SP(TEXT("lowerarm_")+Low).GetTranslation()).GetSafeNormal()*(BW-BE).Size(),Hold[W].GetScale3D());}
            else if(IsBelow(I,W))P[I]=HoldLocal[I]*P[Parents[I]];
            for(const auto Pair:{TPair<FString,FString>(TEXT("UpperLeg_"),TEXT("thigh_")),TPair<FString,FString>(TEXT("LowerLeg_"),TEXT("calf_"))})
            {
                const FString Child=Pair.Key==TEXT("UpperLeg_")?TEXT("LowerLeg_"):TEXT("Foot_");const FString SC=Pair.Value==TEXT("thigh_")?TEXT("calf_"):TEXT("foot_");
                if(I==Index(Pair.Key+Side))P[I].SetRotation((Deform(Pair.Value+Low)*Between(Bind[Index(Child+Side)].GetTranslation()-Bind[I].GetTranslation(),SB(SC+Low).GetTranslation()-SB(Pair.Value+Low).GetTranslation())*Bind[I].GetRotation()).GetNormalized());
            }
            if(I==Index(TEXT("Foot_")+Side))P[I].SetRotation((Deform(TEXT("foot_")+Low)*Bind[I].GetRotation()).GetNormalized());
        }
    }
    for(int I=0;I<Names.Num();++I){if(Names[I].ToString().StartsWith(TEXT("M4_")))P[I]=Hold[I].GetRelativeTransform(Hold[Index(TEXT("DJ_wrist_R"))])*P[Index(TEXT("DJ_wrist_R"))];if(Names[I]!=TEXT("RyanRig")&&Names[I]!=TEXT("Root"))P[I].AddToTranslation(FVector(0,0,FloorShift));}
    // Weapon raw coordinates aren't used by the registration; the common hold
    // assembly below reconstructs every M4 part from the same transform.
    MinReachMargin=1.e6;TrajectoryReachOffset=0;
    if(CompensateReach)
    {
        // Different hip spread/segment ratios can put a mapped swing ankle a
        // few millimetres beyond the target leg's reach. Preserve its authored
        // horizontal path and bring the entire pelvis/body down by the exact
        // feasibility deficit before solving either leg; never stretch a bone.
        for(const FString Side:{FString(TEXT("L")),FString(TEXT("R"))})
        {
            const int U=Index(TEXT("UpperLeg_")+Side),E=Index(TEXT("LowerLeg_")+Side),F=Index(TEXT("Foot_")+Side);
            const FVector Target=Bind[U].GetTranslation()+(SP(TEXT("foot_")+Side.ToLower()).GetTranslation()-SB(TEXT("thigh_")+Side.ToLower()).GetTranslation())*LegScale+FVector(0,0,FloorShift);
            const FVector Hip=P[U].GetTranslation();const double Reach=(Bind[E].GetTranslation()-Bind[U].GetTranslation()).Size()+(Bind[F].GetTranslation()-Bind[E].GetTranslation()).Size()-.15;
            const double XY=FVector::DistSquared2D(Hip,Target);if(XY>=Reach*Reach)return false;
            TrajectoryReachOffset=FMath::Max(TrajectoryReachOffset,Hip.Z-Target.Z-FMath::Sqrt(Reach*Reach-XY));
        }
        if(TrajectoryReachOffset>2)return false;
        for(int I=0;I<P.Num();++I)if(Names[I]!=TEXT("RyanRig")&&Names[I]!=TEXT("Root"))P[I].AddToTranslation(FVector(0,0,-TrajectoryReachOffset));
    }
    for(const FString Side:{FString(TEXT("L")),FString(TEXT("R"))})
    {
        const FString Low=Side.ToLower();const int U=Index(TEXT("UpperLeg_")+Side),E=Index(TEXT("LowerLeg_")+Side),F=Index(TEXT("Foot_")+Side);
        const FVector Hip=P[U].GetTranslation(),Knee=P[E].GetTranslation(),Ankle=P[F].GetTranslation();const FVector Target=Bind[U].GetTranslation()+(SP(TEXT("foot_")+Low).GetTranslation()-SB(TEXT("thigh_")+Low).GetTranslation())*LegScale+FVector(0,0,FloorShift);
        const double A=(Bind[E].GetTranslation()-Bind[U].GetTranslation()).Size(),B=(Bind[F].GetTranslation()-Bind[E].GetTranslation()).Size();FVector NK;
        if(!TwoBone(Hip,Knee,Target,A,B,0,NK))return false;
        P[U].SetRotation((Between(Knee-Hip,NK-Hip)*P[U].GetRotation()).GetNormalized());P[E].SetTranslation(NK);P[E].SetRotation((Between(Ankle-Knee,Target-NK)*P[E].GetRotation()).GetNormalized());
        for(int I=0;I<Names.Num();++I)if(IsBelow(I,F))P[I].AddToTranslation(Target-Ankle);
        MinReachMargin=FMath::Min(MinReachMargin,A+B-(Target-Hip).Size());
    }
    RawMapped=P;
    FQuat Q,Chest;FVector C;Assembly(P,Q,C,Chest);
    Q=(Chest*FitRotation*FQuat::Slerp(AssemblyZero,Q,Amplitude)).GetNormalized();C=P[Index(TEXT("Chest"))].GetTranslation()+Chest.RotateVector(CenterZero+(C-CenterZero)*Amplitude+FitTranslation);
    auto Transfer=[&](int I){return FTransform((Q*Hold[I].GetRotation()).GetNormalized(),C+Q.RotateVector(Hold[I].GetTranslation()-HoldMid),Hold[I].GetScale3D());};
    const FVector JogDelta=Chest.RotateVector(JogTranslation*JogBlend);
    int SideIndex=0;
    for(const FString Side:{FString(TEXT("L")),FString(TEXT("R"))})
    {
        const int U=Index(TEXT("UpperArm_")+Side),E=Index(TEXT("LowerArm_")+Side),W=Index(TEXT("DJ_wrist_")+Side);const FVector Shoulder=P[U].GetTranslation(),Elbow=P[E].GetTranslation();FTransform Target=Transfer(W);
        const double A=(Bind[E].GetTranslation()-Bind[U].GetTranslation()).Size(),B=(Bind[W].GetTranslation()-Bind[E].GetTranslation()).Size();FVector NE;
        const double Pole=PoleAngles[SideIndex]+(LiveClearance&&SideIndex==1?LiveRightPoleDelta:0);
        if(!TwoBone(Shoulder,Elbow,Target.GetTranslation(),A,B,Pole,NE))return false;
        FQuat UpperRotation=(Between(Elbow-Shoulder,NE-Shoulder)*P[U].GetRotation()).GetNormalized();
        if(JogBlend>0)
        {
            // Solve the common contact registration using the already registered
            // source elbow as guide; output the chain once, with unchanged bind.
            const FVector BaselineElbow=NE;Target.AddToTranslation(JogDelta);
            if(!TwoBone(Shoulder,BaselineElbow,Target.GetTranslation(),A,B,JogPole[SideIndex]*JogBlend,NE))return false;
            UpperRotation=(Between(BaselineElbow-Shoulder,NE-Shoulder)*UpperRotation).GetNormalized();
        }
        ++SideIndex;
        const FQuat Hand=Target.GetRotation()*Bind[W].GetRotation().Inverse();const FVector Natural=Hand.RotateVector((Bind[W].GetTranslation()-Bind[E].GetTranslation()).GetSafeNormal());const FQuat DQ=Between(Natural,Target.GetTranslation()-NE)*Hand;
        P[U].SetRotation(UpperRotation);
        for(int I:{E,Index(TEXT("DJ_forearm_")+Side)})P[I]=FTransform((DQ*Bind[I].GetRotation()).GetNormalized(),NE+DQ.RotateVector(Bind[I].GetTranslation()-Bind[E].GetTranslation()),Bind[I].GetScale3D());
        for(int I=0;I<Names.Num();++I)if(IsBelow(I,W)){P[I]=Transfer(I);P[I].AddToTranslation(JogDelta);}
        MinReachMargin=FMath::Min(MinReachMargin,A+B-(Target.GetTranslation()-Shoulder).Size());
    }
    for(int I=0;I<Names.Num();++I)if(Names[I].ToString().StartsWith(TEXT("M4_"))){P[I]=Transfer(I);P[I].AddToTranslation(JogDelta);}
    if(ArmedWeight>1.e-8)
    {
        // The original Rifle graph owns the complete pose and state timing.
        // Register its evaluated assembly in weapon space, then constrain the
        // common hand/weapon assembly to the view only at actual Aiming weight.
        // There is no independent hand offset, stretched limb or input-edge snap.
        FQuat RawQ,RawChest;FVector RawC;Assembly(RawMapped,RawQ,RawC,RawChest);
        RawQ=RawChest*RawQ;RawC=RawMapped[Index(TEXT("Chest"))].GetTranslation()+RawChest.RotateVector(RawC);
        // The reference rifle is attached to hand_r. The palm/span frame used
        // to fit two different grips is NOT its weapon orientation; using that
        // frame let clearance fitting turn low-ready into an unwanted high-ready.
        const FQuat ArmedQ=(SP(TEXT("hand_r")).GetRotation()*AimRotation).GetNormalized();
        const FQuat HeldQ=P[Index(TEXT("M4_body"))].GetRotation()*Hold[Index(TEXT("M4_body"))].GetRotation().Inverse();
        const FVector HeldC=P[Index(TEXT("M4_body"))].GetTranslation()-HeldQ.RotateVector(Hold[Index(TEXT("M4_body"))].GetTranslation()-HoldMid);
        FQuat FinalQ=FQuat::Slerp(HeldQ,ArmedQ,ArmedWeight).GetNormalized();
        const FVector Forward=Hold[Index(TEXT("M4_frontsight"))].GetTranslation()-Hold[Index(TEXT("M4_rearsight"))].GetTranslation();
        const FVector Up=Hold[Index(TEXT("M4_sightup"))].GetTranslation()-Hold[Index(TEXT("M4_rearsight"))].GetTranslation();
        const FQuat ViewQ=Frame(ViewDirection,FVector::UpVector)*Frame(Forward,Up).Inverse();
        FinalQ=FQuat::Slerp(FinalQ,ViewQ,AimWeight).GetNormalized();
        // Register around the evaluated source trigger hand. An unconstrained
        // midpoint fit lowered the rifle below the shoulder to reduce wrist
        // angles; that numerically feasible result failed visual review.
        const int RightWrist=Index(TEXT("DJ_wrist_R"));
        const double AimFraction=FMath::Clamp(AimWeight/ArmedWeight,0.,1.);
        const FVector ArmedC=RawMapped[RightWrist].GetTranslation()+FinalQ.RotateVector(HoldMid-Hold[RightWrist].GetTranslation())+RawChest.RotateVector(FMath::Lerp(ReadyTranslation,AimTranslation,AimFraction));
        const FVector FinalC=FMath::Lerp(HeldC,ArmedC,ArmedWeight);
        auto Held=[&](int I){return FTransform((FinalQ*Hold[I].GetRotation()).GetNormalized(),FinalC+FinalQ.RotateVector(Hold[I].GetTranslation()-HoldMid),Hold[I].GetScale3D());};
        for(int Side=0;Side<2;++Side)
        {
            const FString Suffix=Side==0?TEXT("L"):TEXT("R");const int U=Index(TEXT("UpperArm_")+Suffix),E=Index(TEXT("LowerArm_")+Suffix),W=Index(TEXT("DJ_wrist_")+Suffix);
            // Static and moving Rifle poses have different elbow clearance. The
            // original evaluated PoseMoving curve owns this registration blend.
            const FVector Shoulder=RawMapped[U].GetTranslation(),Elbow=RawMapped[E].GetTranslation(),Guide=FMath::Lerp(P[E].GetTranslation(),Elbow,ArmedWeight);const FTransform Hand=Held(W);
            const double A=(Bind[E].GetTranslation()-Bind[U].GetTranslation()).Size(),B=(Bind[W].GetTranslation()-Bind[E].GetTranslation()).Size();FVector NE;
            const double BasePole=FMath::Lerp(ReadyPole[Side],AimPole[Side],AimFraction)*ArmedWeight;
            if(!TwoBone(Shoulder,Guide,Hand.GetTranslation(),A,B,BasePole,NE))return false;
            const FQuat HandDelta=Hand.GetRotation()*Bind[W].GetRotation().Inverse();
            const FVector Natural=HandDelta.RotateVector((Bind[W].GetTranslation()-Bind[E].GetTranslation()).GetSafeNormal());
            const double Extra=MovingAimPoleDelta[Side]*AimWeight*MovingWeight;
            if(FMath::Abs(Extra)>1.e-8)
            {
                // The moving clearance adjustment may not spend the wrist's
                // entire bend budget on a different directional pose. Keep the
                // hand/gun fixed and constrain only this extra elbow arc.
                const FVector Axis=(Hand.GetTranslation()-Shoulder).GetSafeNormal();
                const FVector Center=Shoulder+Axis*FVector::DotProduct(NE-Shoulder,Axis),Radius=NE-Center;
                auto Arc=[&](double Weight){return Center+FQuat(Axis,Extra*Weight).RotateVector(Radius);};
                const double Limit=FMath::Cos(FMath::DegreesToRadians(44.5));
                auto Feasible=[&](const FVector& E){return FVector::DotProduct(Natural,(Hand.GetTranslation()-E).GetSafeNormal())>=Limit;};
                if(Feasible(Arc(1)))NE=Arc(1);
                else if(Feasible(NE))
                {
                    double Low=0,High=1;for(int Iteration=0;Iteration<20;++Iteration){const double Mid=(Low+High)*.5;if(Feasible(Arc(Mid)))Low=Mid;else High=Mid;}
                    NE=Arc(Low);
                }
            }
            P[U].SetRotation((Between(Elbow-Shoulder,NE-Shoulder)*RawMapped[U].GetRotation()).GetNormalized());
            const FQuat DQ=Between(Natural,Hand.GetTranslation()-NE)*HandDelta;
            for(int I:{E,Index(TEXT("DJ_forearm_")+Suffix)})P[I]=FTransform((DQ*Bind[I].GetRotation()).GetNormalized(),NE+DQ.RotateVector(Bind[I].GetTranslation()-Bind[E].GetTranslation()),Bind[I].GetScale3D());
            for(int I=0;I<Names.Num();++I)if(IsBelow(I,W))P[I]=Held(I);
            MinReachMargin=FMath::Min(MinReachMargin,A+B-(Hand.GetTranslation()-Shoulder).Size());
        }
        for(int I=0;I<Names.Num();++I)if(Names[I].ToString().StartsWith(TEXT("M4_")))P[I]=Held(I);
    }
    Local.SetNum(P.Num());for(int I=0;I<P.Num();++I){P[I].NormalizeRotation();Local[I]=Parents[I]>=0?P[I].GetRelativeTransform(P[Parents[I]]):P[I];if(Local[I].ContainsNaN())return false;}
    return true;
}

bool FLiveCarryRetarget::PlaceSoles(double LeftLock,double RightLock,TArray<FTransform>& Local,TArray<FTransform>& P)
{
    // Source foot bones and boot surfaces differ. Reuse the source contact
    // curves, then solve only the target leg against its actual skinned sole.
    // Swing keeps its authored path except for preventing ground penetration.
    MaxSoleCorrection=0;
    if(!InReachPass)PelvisReachOffset=0;
    for(int Side=0;Side<2;++Side)
    {
        const FString S=Side==0?TEXT("L"):TEXT("R");const int U=TargetIndex.FindChecked(FName(*(TEXT("UpperLeg_")+S))),E=TargetIndex.FindChecked(FName(*(TEXT("LowerLeg_")+S))),F=TargetIndex.FindChecked(FName(*(TEXT("Foot_")+S)));
        auto SoleHeight=[&](const TArray<FTransform>& Pose){double Z=1.e9;for(const auto& Point:SolePoints[Side]){double H=0;for(const auto& Influence:Point)H+=Pose[Influence.Bone].TransformPosition(Influence.Local).Z*Influence.Weight;Z=FMath::Min(Z,H);}return Z;};
        const double Z=SoleHeight(P);
        const double Lock=FMath::Clamp(Side==0?LeftLock:RightLock,0.,1.);
        const double Desired=FMath::Max(-.68,Z+Lock*(-.68-Z));
        const FVector Hip=P[U].GetTranslation(),Knee=P[E].GetTranslation(),Ankle=P[F].GetTranslation();
        const double A=(Bind[E].GetTranslation()-Bind[U].GetTranslation()).Size(),B=(Bind[F].GetTranslation()-Bind[E].GetTranslation()).Size();
        const double Horizontal=FVector::DistSquared2D(Hip,Ankle),Reach=A+B-.15;
        if(Horizontal>=Reach*Reach)return false;
        auto Solve=[&](double Height,TArray<FTransform>& Out)
        {
            FVector Target=Ankle;Target.Z=Height;FVector NK;if(!TwoBone(Hip,Knee,Target,A,B,0,NK))return false;
            Out=P;Out[U].SetRotation((Between(Knee-Hip,NK-Hip)*P[U].GetRotation()).GetNormalized());Out[E].SetTranslation(NK);Out[E].SetRotation((Between(Ankle-Knee,Target-NK)*P[E].GetRotation()).GetNormalized());
            for(int I=0;I<Names.Num();++I)if(IsBelow(I,F))Out[I].AddToTranslation(Target-Ankle);return true;
        };
        // Bracket an actual surface-height root. No bone stretching and no
        // heuristic knee offset; every trial retains the source hinge plane.
        double Low=FMath::Max(Ankle.Z-10,Hip.Z-FMath::Sqrt(Reach*Reach-Horizontal)),High=Ankle.Z+10;
        TArray<FTransform> Trial;if(!Solve(Low,Trial))return false;
        if(SoleHeight(Trial)<Desired)
        {
            if(!Solve(High,Trial)||SoleHeight(Trial)<Desired)return false;
            for(int Iteration=0;Iteration<14;++Iteration){const double Mid=(Low+High)*.5;if(!Solve(Mid,Trial))return false;if(SoleHeight(Trial)<Desired)Low=Mid;else High=Mid;}
            if(!Solve(High,Trial))return false;
        }
        MaxSoleCorrection=FMath::Max(MaxSoleCorrection,FMath::Abs(Trial[F].GetTranslation().Z-Ankle.Z));P=MoveTemp(Trial);
    }
    if(CompensateReach&&!InReachPass)
    {
        double Offset=0;
        for(int Side=0;Side<2;++Side)if((Side==0?LeftLock:RightLock)>.999)
        {
            double Sole=1.e9;for(const auto& Point:SolePoints[Side]){double Z=0;for(const auto& Inf:Point)Z+=P[Inf.Bone].TransformPosition(Inf.Local).Z*Inf.Weight;Sole=FMath::Min(Sole,Z);}
            Offset=FMath::Max(Offset,Sole+.68);
        }
        if(Offset>.02)
        {
            // A planted target boot cannot reach the floor near extension.
            // Move the full pelvis/body chain down by the measured deficit,
            // then re-place both feet at unchanged limb lengths. Never stretch.
            if(Offset>1)return false;
            PelvisReachOffset=Offset;for(int I=0;I<P.Num();++I)if(Names[I]!=TEXT("RyanRig")&&Names[I]!=TEXT("Root"))P[I].AddToTranslation(FVector(0,0,-Offset));
            const double Previous=MaxSoleCorrection;InReachPass=true;const bool Valid=PlaceSoles(LeftLock,RightLock,Local,P);InReachPass=false;MaxSoleCorrection=FMath::Max(Previous,MaxSoleCorrection);return Valid;
        }
    }
    for(int I=0;I<P.Num();++I)Local[I]=Parents[I]>=0?P[I].GetRelativeTransform(P[Parents[I]]):P[I];
    return true;
}
